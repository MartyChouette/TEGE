#include "Enjin/Editor/FlashTimeline.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Tween.h"
#include "Enjin/Animation/Timeline.h"
#include "Enjin/Logging/Log.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>

namespace Enjin {
namespace Editor {

// ============================================================================
// FlashTimelineLayer helpers
// ============================================================================

const FlashKeyframe* FlashTimelineLayer::GetKeyframeAt(u32 frame) const {
    const FlashKeyframe* best = nullptr;
    for (auto& kf : keyframes) {
        if (kf.frameIndex <= frame) {
            if (!best || kf.frameIndex > best->frameIndex) {
                best = &kf;
            }
        }
    }
    return best;
}

FlashKeyframe* FlashTimelineLayer::GetKeyframeAt(u32 frame) {
    FlashKeyframe* best = nullptr;
    for (auto& kf : keyframes) {
        if (kf.frameIndex <= frame) {
            if (!best || kf.frameIndex > best->frameIndex) {
                best = &kf;
            }
        }
    }
    return best;
}

FlashKeyframe& FlashTimelineLayer::GetOrCreateKeyframe(u32 frame) {
    for (auto& kf : keyframes) {
        if (kf.frameIndex == frame) return kf;
    }
    FlashKeyframe newKf;
    newKf.frameIndex = frame;

    // Inherit transform from the previous keyframe if available
    auto* prev = GetKeyframeAt(frame);
    if (prev) {
        newKf.position = prev->position;
        newKf.rotation = prev->rotation;
        newKf.scale = prev->scale;
        newKf.alpha = prev->alpha;
        newKf.visible = prev->visible;
    }

    keyframes.push_back(newKf);

    // Keep sorted
    std::sort(keyframes.begin(), keyframes.end(),
              [](const FlashKeyframe& a, const FlashKeyframe& b) {
                  return a.frameIndex < b.frameIndex;
              });

    for (auto& kf : keyframes) {
        if (kf.frameIndex == frame) return kf;
    }
    // Safety: should never be empty since we just pushed a keyframe above
    if (keyframes.empty()) {
        ENJIN_LOG_WARN(Editor, "FlashTimeline: GetOrCreateKeyframe fallback on empty keyframes vector");
        keyframes.push_back(newKf);
    }
    return keyframes.back();
}

void FlashTimelineLayer::RemoveKeyframe(u32 frame) {
    keyframes.erase(
        std::remove_if(keyframes.begin(), keyframes.end(),
                        [frame](const FlashKeyframe& kf) { return kf.frameIndex == frame; }),
        keyframes.end()
    );
}

void FlashTimelineLayer::GetInterpolatedTransform(u32 frame, Math::Vector3& pos,
                                                    Math::Vector3& rot, Math::Vector3& scl,
                                                    f32& alpha) const {
    if (keyframes.empty()) {
        pos = Math::Vector3(0, 0, 0);
        rot = Math::Vector3(0, 0, 0);
        scl = Math::Vector3(1, 1, 1);
        alpha = 1.0f;
        return;
    }

    // Find surrounding keyframes
    const FlashKeyframe* prev = nullptr;
    const FlashKeyframe* next = nullptr;

    for (auto& kf : keyframes) {
        if (kf.frameIndex <= frame) prev = &kf;
        if (kf.frameIndex > frame && !next) next = &kf;
    }

    if (!prev) prev = &keyframes.front();
    if (!next || !prev->tweenMotion) {
        // No interpolation — use prev keyframe directly
        pos = prev->position;
        rot = prev->rotation;
        scl = prev->scale;
        alpha = prev->alpha;
        return;
    }

    // Interpolate between prev and next
    f32 range = static_cast<f32>(next->frameIndex - prev->frameIndex);
    f32 t = (range > 0) ? static_cast<f32>(frame - prev->frameIndex) / range : 0.0f;
    t = std::clamp(t, 0.0f, 1.0f);

    // Apply easing (25 easing functions via TweenComponent library)
    t = ECS::ApplyEasing(t, prev->tweenEasing);

    // Lerp
    pos = prev->position + (next->position - prev->position) * t;
    rot = prev->rotation + (next->rotation - prev->rotation) * t;
    scl = prev->scale + (next->scale - prev->scale) * t;
    alpha = prev->alpha + (next->alpha - prev->alpha) * t;
}

// ============================================================================
// FlashTimelineData helpers
// ============================================================================

void FlashTimelineData::AddLayer(const std::string& name, ECS::Entity entity) {
    FlashTimelineLayer layer;
    layer.name = name;
    layer.entity = entity;
    layers.push_back(layer);
}

void FlashTimelineData::RemoveLayer(u32 index) {
    if (index < static_cast<u32>(layers.size())) {
        layers.erase(layers.begin() + index);
    }
}

void FlashTimelineData::MoveLayer(u32 from, u32 to) {
    if (from >= static_cast<u32>(layers.size()) || to >= static_cast<u32>(layers.size())) return;
    auto layer = layers[from];
    layers.erase(layers.begin() + from);
    layers.insert(layers.begin() + to, layer);
}

void FlashTimelineData::InsertFrames(u32 atFrame, u32 count) {
    for (auto& layer : layers) {
        for (auto& kf : layer.keyframes) {
            if (kf.frameIndex >= atFrame) {
                kf.frameIndex += count;
            }
        }
    }
    totalFrames += count;
}

void FlashTimelineData::DeleteFrames(u32 atFrame, u32 count) {
    for (auto& layer : layers) {
        layer.keyframes.erase(
            std::remove_if(layer.keyframes.begin(), layer.keyframes.end(),
                            [atFrame, count](const FlashKeyframe& kf) {
                                return kf.frameIndex >= atFrame && kf.frameIndex < atFrame + count;
                            }),
            layer.keyframes.end()
        );
        for (auto& kf : layer.keyframes) {
            if (kf.frameIndex >= atFrame + count) {
                kf.frameIndex -= count;
            }
        }
    }
    if (totalFrames > count) totalFrames -= count;
    else totalFrames = 1;
}

// ============================================================================
// FlashTimelineEditor
// ============================================================================

void FlashTimelineEditor::Play() {
    if (!m_Timeline) return;
    m_Timeline->isPlaying = true;
    m_PlaybackTimer = 0;
}

void FlashTimelineEditor::Pause() {
    if (!m_Timeline) return;
    m_Timeline->isPlaying = false;
}

void FlashTimelineEditor::Stop() {
    if (!m_Timeline) return;
    m_Timeline->isPlaying = false;
    m_Timeline->currentFrame = 0;
    m_PlaybackTimer = 0;
}

void FlashTimelineEditor::StepForward() {
    if (!m_Timeline) return;
    if (m_Timeline->currentFrame < m_Timeline->totalFrames - 1) {
        m_Timeline->currentFrame++;
    } else if (m_Timeline->loop) {
        m_Timeline->currentFrame = 0;
    }
}

void FlashTimelineEditor::StepBackward() {
    if (!m_Timeline) return;
    if (m_Timeline->currentFrame > 0) {
        m_Timeline->currentFrame--;
    }
}

void FlashTimelineEditor::GotoFrame(u32 frame) {
    if (!m_Timeline) return;
    m_Timeline->currentFrame = std::min(frame, m_Timeline->totalFrames - 1);
}

void FlashTimelineEditor::Update(f32 deltaTime) {
    if (!m_Timeline || !m_Timeline->isPlaying) return;

    f32 frameTime = 1.0f / m_Timeline->frameRate;
    m_PlaybackTimer += deltaTime;

    while (m_PlaybackTimer >= frameTime) {
        m_PlaybackTimer -= frameTime;
        StepForward();
    }
}

void FlashTimelineEditor::ApplyFrame(ECS::World* world) {
    if (!m_Timeline || !world) return;

    for (auto& layer : m_Timeline->layers) {
        if (!layer.visible || layer.entity == 0) continue;

        Math::Vector3 pos, rot, scl;
        f32 alpha;
        layer.GetInterpolatedTransform(m_Timeline->currentFrame, pos, rot, scl, alpha);

        auto* tf = world->GetComponent<ECS::TransformComponent>(layer.entity);
        if (tf) {
            tf->position = pos;
            tf->rotation = Math::Quaternion::FromEuler(rot); // Euler to quaternion
            tf->scale = scl;
        }

        auto* sprite = world->GetComponent<ECS::Sprite2DComponent>(layer.entity);
        if (sprite) {
            sprite->alpha = alpha;
        }
    }
}

// ============================================================================
// Render
// ============================================================================

void FlashTimelineEditor::Render(ECS::World* world, const EditorSettings& /*settings*/) {
    if (!m_Timeline) {
        ImGui::TextDisabled("No timeline loaded");
        return;
    }

    DrawToolbar();
    ImGui::Separator();

    // Split: layer list on left, frame grid on right
    f32 layerListWidth = 180.0f;
    f32 availHeight = ImGui::GetContentRegionAvail().y;

    // --- Layer list ---
    ImGui::BeginChild("FlashLayers", ImVec2(layerListWidth, availHeight), true);
    DrawLayerList();
    ImGui::EndChild();

    ImGui::SameLine();

    // --- Frame grid ---
    ImGui::BeginChild("FlashFrameGrid", ImVec2(0, availHeight), true,
                       ImGuiWindowFlags_HorizontalScrollbar);
    DrawFrameGrid();
    ImGui::EndChild();

    // Apply frame to entities
    ApplyFrame(world);
}

void FlashTimelineEditor::DrawToolbar() {
    // Record mode button (red circle, blinking)
    {
        m_RecordBlinkTimer += ImGui::GetIO().DeltaTime;
        if (m_RecordMode) {
            bool blink = std::fmod(m_RecordBlinkTimer, 1.0f) < 0.6f;
            ImGui::PushStyleColor(ImGuiCol_Button, blink ? ImVec4(0.8f, 0.1f, 0.1f, 1.0f) : ImVec4(0.5f, 0.1f, 0.1f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.2f, 0.2f, 1.0f));
            if (ImGui::Button("(R) REC")) { m_RecordMode = false; }
            ImGui::PopStyleColor(2);
        } else {
            if (ImGui::Button("(R)")) { m_RecordMode = true; m_RecordBlinkTimer = 0; }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Record Mode — gizmo moves create keyframes");
        }
    }
    ImGui::SameLine();

    // Transport controls
    if (ImGui::Button("|<")) Stop();
    ImGui::SameLine();
    if (ImGui::Button("<")) StepBackward();
    ImGui::SameLine();

    if (m_Timeline->isPlaying) {
        if (ImGui::Button("||")) Pause();
    } else {
        if (ImGui::Button(">")) Play();
    }
    ImGui::SameLine();
    if (ImGui::Button(">|")) StepForward();

    ImGui::SameLine();
    ImGui::Spacing();
    ImGui::SameLine();

    // Frame counter
    i32 frame = static_cast<i32>(m_Timeline->currentFrame);
    ImGui::SetNextItemWidth(60);
    if (ImGui::DragInt("##Frame", &frame, 0.5f, 0,
                        static_cast<i32>(m_Timeline->totalFrames) - 1)) {
        GotoFrame(static_cast<u32>(frame));
    }
    ImGui::SameLine();
    ImGui::Text("/ %u", m_Timeline->totalFrames);

    ImGui::SameLine();
    ImGui::Spacing();
    ImGui::SameLine();

    // FPS
    ImGui::SetNextItemWidth(50);
    ImGui::DragFloat("FPS", &m_Timeline->frameRate, 0.5f, 1.0f, 120.0f, "%.0f");

    ImGui::SameLine();
    ImGui::Spacing();
    ImGui::SameLine();

    // Loop toggle
    ImGui::Checkbox("Loop", &m_Timeline->loop);

    ImGui::SameLine();
    ImGui::Spacing();
    ImGui::SameLine();

    // Onion skin toggle
    ImGui::Checkbox("Onion Skin", &m_OnionSkin.enabled);

    ImGui::SameLine();
    ImGui::Spacing();
    ImGui::SameLine();

    // Total frames
    i32 total = static_cast<i32>(m_Timeline->totalFrames);
    ImGui::SetNextItemWidth(60);
    if (ImGui::DragInt("Total", &total, 1.0f, 1, 9999)) {
        m_Timeline->totalFrames = static_cast<u32>(std::max(1, total));
    }

    ImGui::SameLine();
    ImGui::Spacing();
    ImGui::SameLine();

    // Add layer button
    if (ImGui::Button("+ Layer")) {
        std::string name = "Layer " + std::to_string(m_Timeline->layers.size() + 1);
        m_Timeline->AddLayer(name);
    }

    // Onion skin settings (collapsible, shown when enabled)
    if (m_OnionSkin.enabled) {
        if (ImGui::TreeNode("Onion Skin Settings")) {
            ImGui::SliderInt("Frames Before", &m_OnionSkin.framesBefore, 0, 10);
            ImGui::SliderInt("Frames After", &m_OnionSkin.framesAfter, 0, 10);
            ImGui::SliderFloat("Opacity", &m_OnionSkin.opacity, 0.05f, 1.0f, "%.2f");
            ImGui::SliderFloat("Falloff", &m_OnionSkin.opacityFalloff, 0.1f, 1.0f, "%.2f");
            ImGui::ColorEdit3("Before Tint", &m_OnionSkin.beforeTint.x);
            ImGui::ColorEdit3("After Tint", &m_OnionSkin.afterTint.x);
            ImGui::TreePop();
        }
    }
}

void FlashTimelineEditor::DrawLayerList() {
    ImGui::Text("Layers");
    ImGui::Separator();

    for (u32 i = 0; i < static_cast<u32>(m_Timeline->layers.size()); ++i) {
        auto& layer = m_Timeline->layers[i];
        ImGui::PushID(static_cast<i32>(i));

        // Visibility toggle
        bool vis = layer.visible;
        if (ImGui::Checkbox("##vis", &vis)) {
            layer.visible = vis;
        }
        ImGui::SameLine();

        // Lock toggle
        bool locked = layer.locked;
        if (ImGui::Checkbox("##lock", &locked)) {
            layer.locked = locked;
        }
        ImGui::SameLine();

        // Layer name (selectable)
        bool isSelected = (m_SelectedLayer == i);
        if (ImGui::Selectable(layer.name.c_str(), isSelected, 0, ImVec2(0, 0))) {
            m_SelectedLayer = i;
        }

        // Context menu
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Rename")) {
                // Would open rename input
            }
            if (ImGui::MenuItem("Delete")) {
                m_Timeline->RemoveLayer(i);
                if (m_SelectedLayer >= static_cast<u32>(m_Timeline->layers.size()) &&
                    !m_Timeline->layers.empty()) {
                    m_SelectedLayer = static_cast<u32>(m_Timeline->layers.size()) - 1;
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Move Up", nullptr, false, i > 0)) {
                m_Timeline->MoveLayer(i, i - 1);
                m_SelectedLayer = i - 1;
            }
            if (ImGui::MenuItem("Move Down", nullptr, false,
                                 i < static_cast<u32>(m_Timeline->layers.size()) - 1)) {
                m_Timeline->MoveLayer(i, i + 1);
                m_SelectedLayer = i + 1;
            }
            ImGui::Separator();
            ImGui::Checkbox("Guide", &layer.isGuide);
            ImGui::Checkbox("Mask", &layer.isMask);
            ImGui::EndPopup();
        }

        ImGui::PopID();
    }
}

f32 FlashTimelineEditor::FrameToX(u32 frame) const {
    f32 cellWidth = 12.0f * m_ZoomLevel;
    return static_cast<f32>(frame) * cellWidth - m_ScrollX;
}

u32 FlashTimelineEditor::XToFrame(f32 x) const {
    f32 cellWidth = 12.0f * m_ZoomLevel;
    f32 frame = (x + m_ScrollX) / cellWidth;
    return static_cast<u32>(std::max(0.0f, frame));
}

ImVec4 FlashTimelineEditor::GetFrameColor(FlashFrameType type) const {
    switch (type) {
        case FlashFrameType::Keyframe:    return ImVec4(0.2f, 0.2f, 0.2f, 1.0f);  // Dark
        case FlashFrameType::StaticFrame: return ImVec4(0.35f, 0.35f, 0.45f, 1.0f);  // Blue-gray
        case FlashFrameType::MotionTween: return ImVec4(0.3f, 0.5f, 0.8f, 1.0f);    // Blue
        case FlashFrameType::ShapeTween:  return ImVec4(0.3f, 0.7f, 0.3f, 1.0f);    // Green
        case FlashFrameType::Empty:
        default:                          return ImVec4(0.9f, 0.9f, 0.9f, 1.0f);    // White
    }
}

void FlashTimelineEditor::DrawFrameGrid() {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    f32 cellWidth = 12.0f * m_ZoomLevel;
    f32 rowHeight = 22.0f;

    u32 numLayers = static_cast<u32>(m_Timeline->layers.size());
    u32 visibleFrames = static_cast<u32>(canvasSize.x / cellWidth) + 2;
    u32 startFrame = static_cast<u32>(m_ScrollX / cellWidth);

    // Frame number header
    f32 headerHeight = 20.0f;
    for (u32 f = startFrame; f < startFrame + visibleFrames && f < m_Timeline->totalFrames; ++f) {
        f32 x = canvasPos.x + FrameToX(f);

        // Draw frame number every 5 frames
        if (f % 5 == 0) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%u", f);
            drawList->AddText(ImVec2(x + 2, canvasPos.y), IM_COL32(180, 180, 180, 255), buf);
        }

        // Tick mark
        drawList->AddLine(
            ImVec2(x, canvasPos.y + headerHeight - 4),
            ImVec2(x, canvasPos.y + headerHeight),
            (f % 5 == 0) ? IM_COL32(150, 150, 150, 255) : IM_COL32(80, 80, 80, 255)
        );
    }

    // Grid rows (one per layer)
    f32 gridTop = canvasPos.y + headerHeight;
    for (u32 layerIdx = 0; layerIdx < numLayers; ++layerIdx) {
        auto& layer = m_Timeline->layers[layerIdx];
        f32 rowY = gridTop + layerIdx * rowHeight;

        // Row background
        ImU32 rowBg = (layerIdx == m_SelectedLayer)
            ? IM_COL32(50, 60, 80, 255)
            : (layerIdx % 2 == 0 ? IM_COL32(30, 32, 38, 255) : IM_COL32(35, 37, 44, 255));
        drawList->AddRectFilled(
            ImVec2(canvasPos.x, rowY),
            ImVec2(canvasPos.x + canvasSize.x, rowY + rowHeight),
            rowBg
        );

        // Draw frame cells
        for (u32 f = startFrame; f < startFrame + visibleFrames && f < m_Timeline->totalFrames; ++f) {
            f32 x = canvasPos.x + FrameToX(f);

            // Determine frame type at this position
            FlashFrameType frameType = FlashFrameType::Empty;
            bool isExactKeyframe = false;

            for (auto& kf : layer.keyframes) {
                if (kf.frameIndex == f) {
                    frameType = kf.type;
                    isExactKeyframe = true;
                    break;
                }
                if (kf.frameIndex < f) {
                    // Check if this keyframe extends to current frame
                    frameType = kf.tweenMotion ? FlashFrameType::MotionTween : FlashFrameType::StaticFrame;
                }
            }

            // Draw cell
            if (frameType != FlashFrameType::Empty) {
                ImVec4 color = GetFrameColor(frameType);
                drawList->AddRectFilled(
                    ImVec2(x + 1, rowY + 1),
                    ImVec2(x + cellWidth - 1, rowY + rowHeight - 1),
                    ImGui::ColorConvertFloat4ToU32(color)
                );
            }

            // Keyframe marker (filled circle)
            if (isExactKeyframe) {
                f32 cx = x + cellWidth * 0.5f;
                f32 cy = rowY + rowHeight * 0.5f;
                drawList->AddCircleFilled(ImVec2(cx, cy), 3.0f, IM_COL32(255, 255, 255, 255));
            }

            // Grid line
            drawList->AddLine(
                ImVec2(x, rowY),
                ImVec2(x, rowY + rowHeight),
                IM_COL32(50, 50, 60, 100)
            );
        }

        // Row border
        drawList->AddLine(
            ImVec2(canvasPos.x, rowY + rowHeight),
            ImVec2(canvasPos.x + canvasSize.x, rowY + rowHeight),
            IM_COL32(60, 60, 70, 255)
        );
    }

    // Draw playhead
    {
        f32 px = canvasPos.x + FrameToX(m_Timeline->currentFrame) + cellWidth * 0.5f;
        drawList->AddLine(
            ImVec2(px, canvasPos.y),
            ImVec2(px, gridTop + numLayers * rowHeight),
            IM_COL32(255, 60, 60, 200),
            2.0f
        );
        // Playhead triangle
        drawList->AddTriangleFilled(
            ImVec2(px - 5, canvasPos.y),
            ImVec2(px + 5, canvasPos.y),
            ImVec2(px, canvasPos.y + 8),
            IM_COL32(255, 60, 60, 255)
        );
    }

    // Handle clicks on the grid
    ImGui::SetCursorScreenPos(canvasPos);
    ImGui::InvisibleButton("FlashGrid", ImVec2(canvasSize.x, headerHeight + numLayers * rowHeight));

    if (ImGui::IsItemHovered()) {
        ImVec2 mousePos = ImGui::GetMousePos();

        // Click in header → move playhead
        if (mousePos.y < gridTop) {
            if (ImGui::IsMouseClicked(0) || (ImGui::IsMouseDown(0) && m_IsDraggingPlayhead)) {
                m_IsDraggingPlayhead = true;
                u32 frame = XToFrame(mousePos.x - canvasPos.x);
                GotoFrame(frame);
            }
        }
        else {
            // Click in grid → select frame/layer
            if (ImGui::IsMouseClicked(0)) {
                u32 layerIdx = static_cast<u32>((mousePos.y - gridTop) / rowHeight);
                u32 frame = XToFrame(mousePos.x - canvasPos.x);
                if (layerIdx < numLayers) {
                    m_SelectedLayer = layerIdx;
                    m_SelectedFrame = frame;
                    GotoFrame(frame);
                }
            }

            // Double-click → insert/toggle keyframe
            if (ImGui::IsMouseDoubleClicked(0)) {
                u32 layerIdx = static_cast<u32>((mousePos.y - gridTop) / rowHeight);
                u32 frame = XToFrame(mousePos.x - canvasPos.x);
                if (layerIdx < numLayers) {
                    auto& layer = m_Timeline->layers[layerIdx];
                    bool found = false;
                    for (auto& kf : layer.keyframes) {
                        if (kf.frameIndex == frame) { found = true; break; }
                    }
                    if (found) {
                        layer.RemoveKeyframe(frame);
                    } else {
                        layer.GetOrCreateKeyframe(frame);
                    }
                }
            }

            // Right-click → context menu
            if (ImGui::IsMouseClicked(1)) {
                u32 layerIdx = static_cast<u32>((mousePos.y - gridTop) / rowHeight);
                u32 frame = XToFrame(mousePos.x - canvasPos.x);
                if (layerIdx < numLayers) {
                    m_SelectedLayer = layerIdx;
                    m_SelectedFrame = frame;
                    ImGui::OpenPopup("FlashFrameContextMenu");
                }
            }
        }

        // Mouse wheel → horizontal scroll
        f32 wheel = ImGui::GetIO().MouseWheel;
        if (ImGui::GetIO().KeyCtrl) {
            // Ctrl+Wheel → zoom
            m_ZoomLevel = std::clamp(m_ZoomLevel + wheel * 0.1f, 0.5f, 3.0f);
        } else if (ImGui::GetIO().KeyShift) {
            // Shift+Wheel → horizontal scroll
            m_ScrollX = std::max(0.0f, m_ScrollX - wheel * cellWidth * 5);
        }
    }

    if (ImGui::IsMouseReleased(0)) {
        m_IsDraggingPlayhead = false;
    }

    // Context menu
    if (ImGui::BeginPopup("FlashFrameContextMenu")) {
        if (m_SelectedLayer < numLayers) {
            auto& layer = m_Timeline->layers[m_SelectedLayer];

            if (ImGui::MenuItem("Insert Keyframe (F6)")) {
                layer.GetOrCreateKeyframe(m_SelectedFrame);
            }
            if (ImGui::MenuItem("Remove Keyframe")) {
                layer.RemoveKeyframe(m_SelectedFrame);
            }
            ImGui::Separator();

            FlashKeyframe* kf = layer.GetKeyframeAt(m_SelectedFrame);
            if (kf && kf->frameIndex == m_SelectedFrame) {
                bool tween = kf->tweenMotion;
                if (ImGui::Checkbox("Motion Tween", &tween)) {
                    kf->tweenMotion = tween;
                    kf->type = tween ? FlashFrameType::MotionTween : FlashFrameType::Keyframe;
                }
                if (tween) {
                    static const char* easingNames[] = {
                        "Linear",
                        "Ease In Quad", "Ease Out Quad", "Ease In-Out Quad",
                        "Ease In Cubic", "Ease Out Cubic", "Ease In-Out Cubic",
                        "Ease In Quart", "Ease Out Quart", "Ease In-Out Quart",
                        "Ease In Sine", "Ease Out Sine", "Ease In-Out Sine",
                        "Ease In Expo", "Ease Out Expo", "Ease In-Out Expo",
                        "Ease In Back", "Ease Out Back", "Ease In-Out Back",
                        "Ease In Elastic", "Ease Out Elastic", "Ease In-Out Elastic",
                        "Ease In Bounce", "Ease Out Bounce", "Ease In-Out Bounce"
                    };
                    i32 easing = static_cast<i32>(kf->tweenEasing);
                    if (ImGui::Combo("Easing", &easing, easingNames, static_cast<i32>(ECS::EasingType::COUNT))) {
                        kf->tweenEasing = static_cast<ECS::EasingType>(easing);
                    }
                }
            }

            ImGui::Separator();
            if (ImGui::MenuItem("Insert Frames", nullptr, false, true)) {
                m_Timeline->InsertFrames(m_SelectedFrame, 5);
            }
            if (ImGui::MenuItem("Delete Frames", nullptr, false, true)) {
                m_Timeline->DeleteFrames(m_SelectedFrame, 5);
            }
        }
        ImGui::EndPopup();
    }

    // --- Keyframe inspector below the grid (if a keyframe is selected) ---
    if (m_SelectedLayer < numLayers) {
        auto& layer = m_Timeline->layers[m_SelectedLayer];
        FlashKeyframe* kf = layer.GetKeyframeAt(m_SelectedFrame);
        if (kf && kf->frameIndex == m_SelectedFrame) {
            ImGui::SetCursorScreenPos(ImVec2(canvasPos.x, gridTop + numLayers * rowHeight + 8));
            DrawKeyframeInspector();
        }
    }
}

void FlashTimelineEditor::DrawPlayhead() {
    // Integrated into DrawFrameGrid
}

void FlashTimelineEditor::DrawKeyframeInspector() {
    if (!m_Timeline || m_SelectedLayer >= static_cast<u32>(m_Timeline->layers.size())) return;

    auto& layer = m_Timeline->layers[m_SelectedLayer];
    FlashKeyframe* kf = layer.GetKeyframeAt(m_SelectedFrame);
    if (!kf || kf->frameIndex != m_SelectedFrame) return;

    ImGui::Text("Keyframe %u — %s", kf->frameIndex, layer.name.c_str());
    ImGui::Separator();

    ImGui::DragFloat3("Position", &kf->position.x, 0.1f);
    ImGui::DragFloat3("Rotation", &kf->rotation.x, 0.5f);
    ImGui::DragFloat3("Scale", &kf->scale.x, 0.01f, 0.01f, 100.0f);
    ImGui::SliderFloat("Alpha", &kf->alpha, 0.0f, 1.0f);
    ImGui::Checkbox("Visible", &kf->visible);

    // Frame label
    char labelBuf[64];
    strncpy(labelBuf, kf->label.c_str(), sizeof(labelBuf) - 1);
    labelBuf[sizeof(labelBuf) - 1] = '\0';
    if (ImGui::InputText("Label", labelBuf, sizeof(labelBuf))) {
        kf->label = labelBuf;
    }

    // Sound
    char soundBuf[128];
    strncpy(soundBuf, kf->soundPath.c_str(), sizeof(soundBuf) - 1);
    soundBuf[sizeof(soundBuf) - 1] = '\0';
    if (ImGui::InputText("Sound", soundBuf, sizeof(soundBuf))) {
        kf->soundPath = soundBuf;
    }
}

void FlashTimelineEditor::DrawSymbolLibrary(ECS::World* /*world*/) {
    ImGui::Text("Symbol Library");
    ImGui::Separator();

    if (!m_Timeline) return;

    for (auto& [name, prefabPath] : m_Timeline->symbolLibrary) {
        ImGui::BulletText("%s -> %s", name.c_str(), prefabPath.c_str());
    }

    if (m_Timeline->symbolLibrary.empty()) {
        ImGui::TextDisabled("No symbols");
    }
}

void FlashTimelineEditor::ConvertToTimeline(ECS::World* world) {
    if (!m_Timeline || !world) return;

    for (auto& layer : m_Timeline->layers) {
        if (layer.entity == 0 || layer.keyframes.empty()) continue;

        // Check if entity already has a TimelineComponent
        auto* tlComp = world->GetComponent<Animation::TimelineComponent>(layer.entity);
        if (!tlComp) {
            world->AddComponent<Animation::TimelineComponent>(layer.entity);
            tlComp = world->GetComponent<Animation::TimelineComponent>(layer.entity);
        }
        if (!tlComp) continue;

        tlComp->duration = static_cast<f32>(m_Timeline->totalFrames) / m_Timeline->frameRate;
        tlComp->loop = m_Timeline->loop;
        tlComp->playbackSpeed = 1.0f;

        // Convert position keyframes
        Animation::PropertyTrack posTrack;
        posTrack.targetProperty = "position";
        posTrack.targetEntity = layer.entity;

        for (auto& kf : layer.keyframes) {
            Animation::PropertyKeyframe pk;
            pk.time = static_cast<f32>(kf.frameIndex) / m_Timeline->frameRate;
            pk.value = kf.position;
            // Map EasingType to TimelineEasing (best-effort for the 4 Timeline easing types)
            switch (kf.tweenEasing) {
                case ECS::EasingType::EaseInQuad:
                case ECS::EasingType::EaseInCubic:
                case ECS::EasingType::EaseInQuart:
                case ECS::EasingType::EaseInSine:
                case ECS::EasingType::EaseInExpo:
                case ECS::EasingType::EaseInBack:
                case ECS::EasingType::EaseInElastic:
                case ECS::EasingType::EaseInBounce:
                    pk.easing = Animation::TimelineEasing::EaseIn; break;
                case ECS::EasingType::EaseOutQuad:
                case ECS::EasingType::EaseOutCubic:
                case ECS::EasingType::EaseOutQuart:
                case ECS::EasingType::EaseOutSine:
                case ECS::EasingType::EaseOutExpo:
                case ECS::EasingType::EaseOutBack:
                case ECS::EasingType::EaseOutElastic:
                case ECS::EasingType::EaseOutBounce:
                    pk.easing = Animation::TimelineEasing::EaseOut; break;
                case ECS::EasingType::EaseInOutQuad:
                case ECS::EasingType::EaseInOutCubic:
                case ECS::EasingType::EaseInOutQuart:
                case ECS::EasingType::EaseInOutSine:
                case ECS::EasingType::EaseInOutExpo:
                case ECS::EasingType::EaseInOutBack:
                case ECS::EasingType::EaseInOutElastic:
                case ECS::EasingType::EaseInOutBounce:
                    pk.easing = Animation::TimelineEasing::EaseInOut; break;
                default:
                    pk.easing = Animation::TimelineEasing::Linear; break;
            }
            posTrack.keyframes.push_back(pk);
        }
        tlComp->propertyTracks.push_back(posTrack);
    }

    ENJIN_LOG_INFO(Editor, "Converted Flash timeline to %zu TimelineComponents",
                   m_Timeline->layers.size());
}

std::vector<OnionSkinGhost> FlashTimelineEditor::ComputeOnionSkinGhosts() const {
    std::vector<OnionSkinGhost> ghosts;
    if (!m_Timeline || !m_OnionSkin.enabled) return ghosts;

    u32 currentFrame = m_Timeline->currentFrame;

    for (const auto& layer : m_Timeline->layers) {
        if (!layer.visible || layer.entity == 0 || layer.keyframes.empty()) continue;

        // Ghost frames before current
        for (i32 i = 1; i <= m_OnionSkin.framesBefore; ++i) {
            i32 ghostFrame = static_cast<i32>(currentFrame) - i;
            if (ghostFrame < 0) continue;

            Math::Vector3 pos, rot, scl;
            f32 alpha;
            layer.GetInterpolatedTransform(static_cast<u32>(ghostFrame), pos, rot, scl, alpha);

            f32 falloff = std::pow(m_OnionSkin.opacityFalloff, static_cast<f32>(i - 1));
            OnionSkinGhost ghost;
            ghost.entity = layer.entity;
            ghost.position = pos;
            ghost.rotation = rot;
            ghost.scale = scl;
            ghost.alpha = alpha;
            ghost.tint = m_OnionSkin.beforeTint;
            ghost.ghostOpacity = m_OnionSkin.opacity * falloff;
            ghosts.push_back(ghost);
        }

        // Ghost frames after current
        for (i32 i = 1; i <= m_OnionSkin.framesAfter; ++i) {
            u32 ghostFrame = currentFrame + static_cast<u32>(i);
            if (ghostFrame >= m_Timeline->totalFrames) continue;

            Math::Vector3 pos, rot, scl;
            f32 alpha;
            layer.GetInterpolatedTransform(ghostFrame, pos, rot, scl, alpha);

            f32 falloff = std::pow(m_OnionSkin.opacityFalloff, static_cast<f32>(i - 1));
            OnionSkinGhost ghost;
            ghost.entity = layer.entity;
            ghost.position = pos;
            ghost.rotation = rot;
            ghost.scale = scl;
            ghost.alpha = alpha;
            ghost.tint = m_OnionSkin.afterTint;
            ghost.ghostOpacity = m_OnionSkin.opacity * falloff;
            ghosts.push_back(ghost);
        }
    }

    return ghosts;
}

void FlashTimelineEditor::CaptureKeyframe(ECS::World* world, ECS::Entity entity) {
    if (!m_Timeline || !world || entity == 0) return;

    // Find the layer for this entity
    FlashTimelineLayer* targetLayer = nullptr;
    for (auto& layer : m_Timeline->layers) {
        if (layer.entity == entity) {
            targetLayer = &layer;
            break;
        }
    }
    if (!targetLayer) return;

    // Read current transform from entity
    auto* tf = world->GetComponent<ECS::TransformComponent>(entity);
    if (!tf) return;

    // Create or update keyframe at current frame
    FlashKeyframe& kf = targetLayer->GetOrCreateKeyframe(m_Timeline->currentFrame);
    kf.position = tf->position;
    kf.rotation = tf->rotation.ToEuler();
    kf.scale = tf->scale;
    kf.type = FlashFrameType::Keyframe;

    // Auto-enable motion tween if there's a previous keyframe
    if (targetLayer->keyframes.size() > 1) {
        // Find the previous keyframe and enable motion tween on it
        for (auto& prevKf : targetLayer->keyframes) {
            if (prevKf.frameIndex < kf.frameIndex) {
                prevKf.tweenMotion = true;
                if (prevKf.type == FlashFrameType::Keyframe)
                    prevKf.type = FlashFrameType::MotionTween;
            }
        }
    }

    // Read sprite alpha if available
    auto* sprite = world->GetComponent<ECS::Sprite2DComponent>(entity);
    if (sprite) {
        kf.alpha = sprite->alpha;
    }

    ENJIN_LOG_INFO(Editor, "Captured keyframe at frame %u for entity %llu",
                   m_Timeline->currentFrame, (unsigned long long)entity);
}

void FlashTimelineEditor::ImportFromSWFSprite(const std::string& /*swfPath*/) {
    ENJIN_LOG_INFO(Editor, "SWF sprite import not yet implemented in timeline editor");
}

} // namespace Editor
} // namespace Enjin
