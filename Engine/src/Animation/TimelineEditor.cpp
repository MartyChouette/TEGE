#include "Enjin/Animation/TimelineEditor.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/ECS/Components/Light.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/Math/Quaternion.h"
#include "Enjin/Logging/Log.h"
#include <algorithm>
#include <cmath>

namespace Enjin {
namespace Animation {

// ============================================================================
// Timeline creation and serialization
// ============================================================================

void TimelineEditor::CreateTimeline(ECS::Entity entity, f32 duration) {
    m_TargetEntity = entity;
    m_State = TimelineEditorState{};
    m_State.duration = duration;

    // Start with one empty layer
    TimelineLayer layer;
    layer.name = "Layer 1";
    m_State.layers.push_back(std::move(layer));

    m_State.selectedLayerIndex = 0;
}

void TimelineEditor::LoadFromComponent(const TimelineComponent& component) {
    m_State = TimelineEditorState{};
    m_State.duration = component.duration;
    m_State.currentTime = component.currentTime;
    m_State.isPlaying = component.isPlaying;
    m_State.loop = component.loop;

    // Create a single layer to hold all property tracks from the component
    TimelineLayer layer;
    layer.name = "Imported";

    for (const auto& pt : component.propertyTracks) {
        EditorTrack track;
        track.name = pt.targetProperty;
        track.propertyPath = pt.targetProperty;
        track.targetEntity = pt.targetEntity;

        // Determine property type from the first keyframe's variant
        if (!pt.keyframes.empty()) {
            const auto& firstVal = pt.keyframes[0].value;
            if (std::holds_alternative<Math::Vector3>(firstVal)) {
                track.propertyType = AnimatedPropertyType::Vector3;
            } else if (std::holds_alternative<bool>(firstVal)) {
                track.propertyType = AnimatedPropertyType::Bool;
            } else {
                track.propertyType = AnimatedPropertyType::Float;
            }
        }

        // Convert PropertyKeyframes to EditorKeyframes
        for (const auto& kf : pt.keyframes) {
            EditorKeyframe ekf;
            ekf.time = kf.time;

            if (auto* fVal = std::get_if<f32>(&kf.value)) {
                ekf.value = *fVal;
            } else if (auto* vVal = std::get_if<Math::Vector3>(&kf.value)) {
                ekf.vectorValue = *vVal;
            } else if (auto* bVal = std::get_if<bool>(&kf.value)) {
                ekf.value = (*bVal) ? 1.0f : 0.0f;
            }

            // Map TimelineEasing to KeyframeInterpolation
            switch (kf.easing) {
                case TimelineEasing::Step:
                    ekf.interpolation = KeyframeInterpolation::Constant;
                    break;
                case TimelineEasing::Linear:
                    ekf.interpolation = KeyframeInterpolation::Linear;
                    break;
                case TimelineEasing::EaseIn:
                case TimelineEasing::EaseOut:
                case TimelineEasing::EaseInOut:
                    ekf.interpolation = KeyframeInterpolation::Bezier;
                    break;
                default:
                    ekf.interpolation = KeyframeInterpolation::Linear;
                    break;
            }

            // Auto-compute tangent handles for imported keyframes
            ekf.tangentInTime = -0.1f;
            ekf.tangentInValue = 0.0f;
            ekf.tangentOutTime = 0.1f;
            ekf.tangentOutValue = 0.0f;
            ekf.tangentsBroken = false;

            track.keyframes.push_back(std::move(ekf));
        }

        layer.tracks.push_back(std::move(track));
    }

    if (layer.tracks.empty()) {
        // Even with no tracks, ensure at least one layer exists
        layer.name = "Layer 1";
    }

    m_State.layers.push_back(std::move(layer));
    m_State.selectedLayerIndex = 0;
}

TimelineComponent TimelineEditor::SaveToComponent() const {
    TimelineComponent component;
    component.duration = m_State.duration;
    component.currentTime = m_State.currentTime;
    component.isPlaying = m_State.isPlaying;
    component.loop = m_State.loop;
    component.playbackSpeed = 1.0f;

    // Flatten all layers into property tracks
    for (const auto& layer : m_State.layers) {
        for (const auto& track : layer.tracks) {
            PropertyTrack pt;
            pt.targetProperty = track.propertyPath;
            pt.targetEntity = track.targetEntity;
            pt.enabled = track.visible && !track.locked;

            for (const auto& ekf : track.keyframes) {
                PropertyKeyframe kf;
                kf.time = ekf.time;

                // Convert value back to variant
                switch (track.propertyType) {
                    case AnimatedPropertyType::Float:
                        kf.value = ekf.value;
                        break;
                    case AnimatedPropertyType::Vector3:
                    case AnimatedPropertyType::Color:
                        kf.value = ekf.vectorValue;
                        break;
                    case AnimatedPropertyType::Bool:
                        kf.value = (ekf.value > 0.5f);
                        break;
                }

                // Map KeyframeInterpolation back to TimelineEasing
                switch (ekf.interpolation) {
                    case KeyframeInterpolation::Constant:
                        kf.easing = TimelineEasing::Step;
                        break;
                    case KeyframeInterpolation::Linear:
                        kf.easing = TimelineEasing::Linear;
                        break;
                    case KeyframeInterpolation::Bezier:
                        kf.easing = TimelineEasing::EaseInOut;
                        break;
                    case KeyframeInterpolation::CatmullRom:
                        kf.easing = TimelineEasing::EaseInOut;
                        break;
                    default:
                        kf.easing = TimelineEasing::Linear;
                        break;
                }

                pt.keyframes.push_back(std::move(kf));
            }

            component.propertyTracks.push_back(std::move(pt));
        }
    }

    return component;
}

// ============================================================================
// Layer management
// ============================================================================

void TimelineEditor::AddLayer(const std::string& name) {
    TimelineLayer layer;
    layer.name = name;
    m_State.layers.push_back(std::move(layer));
    m_State.selectedLayerIndex = static_cast<i32>(m_State.layers.size()) - 1;
}

void TimelineEditor::RemoveLayer(u32 index) {
    if (index >= static_cast<u32>(m_State.layers.size())) return;
    // Always keep at least one layer
    if (m_State.layers.size() <= 1) return;

    m_State.layers.erase(m_State.layers.begin() + index);

    // Fix selection
    if (m_State.selectedLayerIndex >= static_cast<i32>(m_State.layers.size())) {
        m_State.selectedLayerIndex = static_cast<i32>(m_State.layers.size()) - 1;
    }
}

void TimelineEditor::MoveLayer(u32 fromIndex, u32 toIndex) {
    if (fromIndex >= static_cast<u32>(m_State.layers.size())) return;
    if (toIndex >= static_cast<u32>(m_State.layers.size())) return;
    if (fromIndex == toIndex) return;

    TimelineLayer moved = std::move(m_State.layers[fromIndex]);
    m_State.layers.erase(m_State.layers.begin() + fromIndex);

    // Adjust toIndex if it was after the removed element
    if (toIndex > fromIndex) --toIndex;
    m_State.layers.insert(m_State.layers.begin() + toIndex, std::move(moved));

    m_State.selectedLayerIndex = static_cast<i32>(toIndex);
}

// ============================================================================
// Track management
// ============================================================================

void TimelineEditor::AddTrack(u32 layerIndex, const std::string& propertyPath,
                              AnimatedPropertyType type, ECS::Entity entity) {
    if (layerIndex >= static_cast<u32>(m_State.layers.size())) return;

    EditorTrack track;
    track.name = propertyPath;
    track.propertyPath = propertyPath;
    track.targetEntity = entity;
    track.propertyType = type;

    // Assign a color based on property type for curve editor readability
    switch (type) {
        case AnimatedPropertyType::Float:
            track.curveColor = Math::Vector3(0.2f, 0.8f, 0.4f);
            break;
        case AnimatedPropertyType::Vector3:
            track.curveColor = Math::Vector3(0.4f, 0.6f, 1.0f);
            break;
        case AnimatedPropertyType::Color:
            track.curveColor = Math::Vector3(1.0f, 0.6f, 0.2f);
            break;
        case AnimatedPropertyType::Bool:
            track.curveColor = Math::Vector3(0.8f, 0.2f, 0.8f);
            break;
    }

    m_State.layers[layerIndex].tracks.push_back(std::move(track));
}

void TimelineEditor::RemoveTrack(u32 layerIndex, u32 trackIndex) {
    if (layerIndex >= static_cast<u32>(m_State.layers.size())) return;
    auto& tracks = m_State.layers[layerIndex].tracks;
    if (trackIndex >= static_cast<u32>(tracks.size())) return;

    tracks.erase(tracks.begin() + trackIndex);
}

// ============================================================================
// Keyframe operations
// ============================================================================

void TimelineEditor::AddKeyframe(u32 layerIndex, u32 trackIndex, f32 time, f32 value) {
    if (layerIndex >= static_cast<u32>(m_State.layers.size())) return;
    auto& tracks = m_State.layers[layerIndex].tracks;
    if (trackIndex >= static_cast<u32>(tracks.size())) return;

    // Snap to frame grid if enabled
    if (m_State.snapToFrames) {
        time = SnapToFrame(time);
    }

    auto& keyframes = tracks[trackIndex].keyframes;

    // Check if a keyframe already exists at this time (within half-frame tolerance)
    f32 frameTolerance = 0.5f / m_State.fps;
    for (auto& kf : keyframes) {
        if (std::abs(kf.time - time) < frameTolerance) {
            // Update existing keyframe value
            kf.value = value;
            return;
        }
    }

    // Create new keyframe
    EditorKeyframe kf;
    kf.time = time;
    kf.value = value;
    kf.interpolation = KeyframeInterpolation::Linear;

    // Auto-compute tangent handles from neighboring keyframes
    if (!keyframes.empty()) {
        // Find neighboring keyframes for tangent estimation
        f32 prevVal = value;
        f32 nextVal = value;
        f32 prevTime = time - 1.0f;
        f32 nextTime = time + 1.0f;

        for (const auto& existing : keyframes) {
            if (existing.time < time) {
                prevVal = existing.value;
                prevTime = existing.time;
            }
        }
        for (const auto& existing : keyframes) {
            if (existing.time > time) {
                nextVal = existing.value;
                nextTime = existing.time;
                break;
            }
        }

        // Compute a gentle slope for the tangent handles
        f32 dt = nextTime - prevTime;
        if (dt > 0.0001f) {
            f32 slope = (nextVal - prevVal) / dt;
            kf.tangentInTime = -0.1f;
            kf.tangentInValue = -slope * 0.1f;
            kf.tangentOutTime = 0.1f;
            kf.tangentOutValue = slope * 0.1f;
        }
    }

    // Insert in sorted order by time
    auto insertPos = std::lower_bound(
        keyframes.begin(), keyframes.end(), kf,
        [](const EditorKeyframe& a, const EditorKeyframe& b) {
            return a.time < b.time;
        });

    keyframes.insert(insertPos, std::move(kf));
}

void TimelineEditor::AddKeyframeVec3(u32 layerIndex, u32 trackIndex,
                                     f32 time, const Math::Vector3& value) {
    if (layerIndex >= static_cast<u32>(m_State.layers.size())) return;
    auto& tracks = m_State.layers[layerIndex].tracks;
    if (trackIndex >= static_cast<u32>(tracks.size())) return;

    // Snap to frame grid if enabled
    if (m_State.snapToFrames) {
        time = SnapToFrame(time);
    }

    auto& keyframes = tracks[trackIndex].keyframes;

    // Check if a keyframe already exists at this time
    f32 frameTolerance = 0.5f / m_State.fps;
    for (auto& kf : keyframes) {
        if (std::abs(kf.time - time) < frameTolerance) {
            kf.vectorValue = value;
            return;
        }
    }

    // Create new keyframe
    EditorKeyframe kf;
    kf.time = time;
    kf.vectorValue = value;
    kf.interpolation = KeyframeInterpolation::Linear;
    kf.tangentInTime = -0.1f;
    kf.tangentInValue = 0.0f;
    kf.tangentOutTime = 0.1f;
    kf.tangentOutValue = 0.0f;

    // Insert in sorted order by time
    auto insertPos = std::lower_bound(
        keyframes.begin(), keyframes.end(), kf,
        [](const EditorKeyframe& a, const EditorKeyframe& b) {
            return a.time < b.time;
        });

    keyframes.insert(insertPos, std::move(kf));
}

void TimelineEditor::RemoveKeyframe(u32 layerIndex, u32 trackIndex, u32 keyframeIndex) {
    if (layerIndex >= static_cast<u32>(m_State.layers.size())) return;
    auto& tracks = m_State.layers[layerIndex].tracks;
    if (trackIndex >= static_cast<u32>(tracks.size())) return;
    auto& keyframes = tracks[trackIndex].keyframes;
    if (keyframeIndex >= static_cast<u32>(keyframes.size())) return;

    keyframes.erase(keyframes.begin() + keyframeIndex);
}

void TimelineEditor::MoveKeyframe(u32 layerIndex, u32 trackIndex,
                                  u32 keyframeIndex, f32 newTime) {
    if (layerIndex >= static_cast<u32>(m_State.layers.size())) return;
    auto& tracks = m_State.layers[layerIndex].tracks;
    if (trackIndex >= static_cast<u32>(tracks.size())) return;
    auto& keyframes = tracks[trackIndex].keyframes;
    if (keyframeIndex >= static_cast<u32>(keyframes.size())) return;

    // Snap to frame grid if enabled
    if (m_State.snapToFrames) {
        newTime = SnapToFrame(newTime);
    }

    // Clamp within timeline range
    newTime = std::clamp(newTime, 0.0f, m_State.duration);

    // Extract the keyframe, remove from its current position, re-insert sorted
    EditorKeyframe kf = std::move(keyframes[keyframeIndex]);
    kf.time = newTime;
    keyframes.erase(keyframes.begin() + keyframeIndex);

    auto insertPos = std::lower_bound(
        keyframes.begin(), keyframes.end(), kf,
        [](const EditorKeyframe& a, const EditorKeyframe& b) {
            return a.time < b.time;
        });

    keyframes.insert(insertPos, std::move(kf));
}

// ============================================================================
// Auto-key
// ============================================================================

void TimelineEditor::AutoKey(ECS::World* world) {
    if (!world) return;

    for (u32 li = 0; li < static_cast<u32>(m_State.layers.size()); ++li) {
        auto& layer = m_State.layers[li];
        if (layer.locked) continue;

        for (u32 ti = 0; ti < static_cast<u32>(layer.tracks.size()); ++ti) {
            auto& track = layer.tracks[ti];
            if (track.locked || !track.visible) continue;
            if (!world->IsValid(track.targetEntity)) continue;

            switch (track.propertyType) {
                case AnimatedPropertyType::Float:
                case AnimatedPropertyType::Bool: {
                    f32 currentVal = ReadPropertyFloat(world, track.targetEntity, track.propertyPath);
                    AddKeyframe(li, ti, m_State.currentTime, currentVal);
                    break;
                }
                case AnimatedPropertyType::Vector3:
                case AnimatedPropertyType::Color: {
                    Math::Vector3 currentVal = ReadPropertyVec3(world, track.targetEntity, track.propertyPath);
                    AddKeyframeVec3(li, ti, m_State.currentTime, currentVal);
                    break;
                }
            }
        }
    }
}

// ============================================================================
// Evaluation
// ============================================================================

void TimelineEditor::Evaluate(ECS::World* world, f32 time) {
    if (!world) return;

    for (const auto& layer : m_State.layers) {
        if (!layer.visible) continue;

        for (const auto& track : layer.tracks) {
            if (track.locked || !track.visible) continue;
            if (track.keyframes.empty()) continue;
            if (!world->IsValid(track.targetEntity)) continue;

            f32 scalarValue = 0.0f;
            Math::Vector3 vecValue;

            switch (track.propertyType) {
                case AnimatedPropertyType::Float:
                case AnimatedPropertyType::Bool:
                    scalarValue = EvaluateCurve(track, time);
                    break;
                case AnimatedPropertyType::Vector3:
                case AnimatedPropertyType::Color:
                    vecValue = EvaluateCurveVec3(track, time);
                    break;
            }

            ApplyPropertyValue(world, track.targetEntity, track.propertyPath,
                             track.propertyType, scalarValue, vecValue);
        }
    }
}

f32 TimelineEditor::EvaluateCurve(const EditorTrack& track, f32 time) const {
    const auto& keyframes = track.keyframes;
    if (keyframes.empty()) return 0.0f;

    // Before first keyframe: hold first value
    if (time <= keyframes.front().time) {
        return keyframes.front().value;
    }

    // After last keyframe: hold last value
    if (time >= keyframes.back().time) {
        return keyframes.back().value;
    }

    // Find the two surrounding keyframes
    usize idxAfter = 0;
    for (usize i = 0; i < keyframes.size(); ++i) {
        if (keyframes[i].time > time) {
            idxAfter = i;
            break;
        }
    }

    usize idxBefore = (idxAfter > 0) ? (idxAfter - 1) : 0;
    const auto& kfA = keyframes[idxBefore];
    const auto& kfB = keyframes[idxAfter];

    f32 segmentDuration = kfB.time - kfA.time;
    if (segmentDuration <= 0.0f) return kfA.value;

    f32 t = (time - kfA.time) / segmentDuration;
    t = std::clamp(t, 0.0f, 1.0f);

    // Apply interpolation based on the destination keyframe's mode
    switch (kfB.interpolation) {
        case KeyframeInterpolation::Constant:
            return kfA.value;

        case KeyframeInterpolation::Linear:
            return kfA.value + (kfB.value - kfA.value) * t;

        case KeyframeInterpolation::Bezier: {
            // Cubic bezier: p0 = kfA value, p3 = kfB value
            // p1 = kfA value + outTangent, p2 = kfB value + inTangent
            f32 p0 = kfA.value;
            f32 p1 = kfA.value + kfA.tangentOutValue;
            f32 p2 = kfB.value + kfB.tangentInValue;
            f32 p3 = kfB.value;
            return EvaluateBezier(t, p0, p1, p2, p3);
        }

        case KeyframeInterpolation::CatmullRom: {
            // Need keyframes before-before and after-after for CatmullRom
            f32 p0 = kfA.value; // Default: repeat endpoints
            f32 p1 = kfA.value;
            f32 p2 = kfB.value;
            f32 p3 = kfB.value;

            if (idxBefore > 0) {
                p0 = keyframes[idxBefore - 1].value;
            }
            if (idxAfter + 1 < keyframes.size()) {
                p3 = keyframes[idxAfter + 1].value;
            }

            return EvaluateCatmullRom(t, p0, p1, p2, p3);
        }

        default:
            return kfA.value + (kfB.value - kfA.value) * t;
    }
}

Math::Vector3 TimelineEditor::EvaluateCurveVec3(const EditorTrack& track, f32 time) const {
    const auto& keyframes = track.keyframes;
    if (keyframes.empty()) return Math::Vector3(0.0f);

    // Before first keyframe: hold first value
    if (time <= keyframes.front().time) {
        return keyframes.front().vectorValue;
    }

    // After last keyframe: hold last value
    if (time >= keyframes.back().time) {
        return keyframes.back().vectorValue;
    }

    // Find the two surrounding keyframes
    usize idxAfter = 0;
    for (usize i = 0; i < keyframes.size(); ++i) {
        if (keyframes[i].time > time) {
            idxAfter = i;
            break;
        }
    }

    usize idxBefore = (idxAfter > 0) ? (idxAfter - 1) : 0;
    const auto& kfA = keyframes[idxBefore];
    const auto& kfB = keyframes[idxAfter];

    f32 segmentDuration = kfB.time - kfA.time;
    if (segmentDuration <= 0.0f) return kfA.vectorValue;

    f32 t = (time - kfA.time) / segmentDuration;
    t = std::clamp(t, 0.0f, 1.0f);

    const Math::Vector3& vA = kfA.vectorValue;
    const Math::Vector3& vB = kfB.vectorValue;

    switch (kfB.interpolation) {
        case KeyframeInterpolation::Constant:
            return vA;

        case KeyframeInterpolation::Linear:
            return Math::Vector3(
                vA.x + (vB.x - vA.x) * t,
                vA.y + (vB.y - vA.y) * t,
                vA.z + (vB.z - vA.z) * t
            );

        case KeyframeInterpolation::Bezier: {
            // Evaluate bezier per-component
            Math::Vector3 result;
            result.x = EvaluateBezier(t, vA.x, vA.x + kfA.tangentOutValue, vB.x + kfB.tangentInValue, vB.x);
            result.y = EvaluateBezier(t, vA.y, vA.y + kfA.tangentOutValue, vB.y + kfB.tangentInValue, vB.y);
            result.z = EvaluateBezier(t, vA.z, vA.z + kfA.tangentOutValue, vB.z + kfB.tangentInValue, vB.z);
            return result;
        }

        case KeyframeInterpolation::CatmullRom: {
            Math::Vector3 p0 = vA;
            Math::Vector3 p3 = vB;

            if (idxBefore > 0) {
                p0 = keyframes[idxBefore - 1].vectorValue;
            }
            if (idxAfter + 1 < keyframes.size()) {
                p3 = keyframes[idxAfter + 1].vectorValue;
            }

            Math::Vector3 result;
            result.x = EvaluateCatmullRom(t, p0.x, vA.x, vB.x, p3.x);
            result.y = EvaluateCatmullRom(t, p0.y, vA.y, vB.y, p3.y);
            result.z = EvaluateCatmullRom(t, p0.z, vA.z, vB.z, p3.z);
            return result;
        }

        default:
            return Math::Vector3(
                vA.x + (vB.x - vA.x) * t,
                vA.y + (vB.y - vA.y) * t,
                vA.z + (vB.z - vA.z) * t
            );
    }
}

// ============================================================================
// Curve math helpers
// ============================================================================

f32 TimelineEditor::EvaluateBezier(f32 t, f32 p0, f32 p1, f32 p2, f32 p3) const {
    // Standard cubic bezier: B(t) = (1-t)^3*p0 + 3*(1-t)^2*t*p1 + 3*(1-t)*t^2*p2 + t^3*p3
    f32 oneMinusT = 1.0f - t;
    f32 oneMinusT2 = oneMinusT * oneMinusT;
    f32 oneMinusT3 = oneMinusT2 * oneMinusT;
    f32 t2 = t * t;
    f32 t3 = t2 * t;

    return oneMinusT3 * p0
         + 3.0f * oneMinusT2 * t * p1
         + 3.0f * oneMinusT * t2 * p2
         + t3 * p3;
}

f32 TimelineEditor::EvaluateCatmullRom(f32 t, f32 p0, f32 p1, f32 p2, f32 p3) const {
    // Catmull-Rom: 0.5 * ((2*p1) + (-p0+p2)*t + (2p0-5p1+4p2-p3)*t^2 + (-p0+3p1-3p2+p3)*t^3)
    f32 t2 = t * t;
    f32 t3 = t2 * t;

    return 0.5f * ((2.0f * p1)
        + (-p0 + p2) * t
        + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2
        + (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
}

// ============================================================================
// Property application
// ============================================================================

void TimelineEditor::ApplyPropertyValue(ECS::World* world, ECS::Entity entity,
                                        const std::string& propertyPath,
                                        AnimatedPropertyType type,
                                        f32 scalarValue, const Math::Vector3& vecValue) {
    if (!world) return;

    // --- Transform properties ---
    if (propertyPath == "transform.position") {
        auto* transform = world->GetComponent<ECS::TransformComponent>(entity);
        if (transform) transform->position = vecValue;
    }
    else if (propertyPath == "transform.position.x") {
        auto* transform = world->GetComponent<ECS::TransformComponent>(entity);
        if (transform) transform->position.x = scalarValue;
    }
    else if (propertyPath == "transform.position.y") {
        auto* transform = world->GetComponent<ECS::TransformComponent>(entity);
        if (transform) transform->position.y = scalarValue;
    }
    else if (propertyPath == "transform.position.z") {
        auto* transform = world->GetComponent<ECS::TransformComponent>(entity);
        if (transform) transform->position.z = scalarValue;
    }
    else if (propertyPath == "transform.scale") {
        auto* transform = world->GetComponent<ECS::TransformComponent>(entity);
        if (transform) transform->scale = vecValue;
    }
    else if (propertyPath == "transform.scale.x") {
        auto* transform = world->GetComponent<ECS::TransformComponent>(entity);
        if (transform) transform->scale.x = scalarValue;
    }
    else if (propertyPath == "transform.scale.y") {
        auto* transform = world->GetComponent<ECS::TransformComponent>(entity);
        if (transform) transform->scale.y = scalarValue;
    }
    else if (propertyPath == "transform.scale.z") {
        auto* transform = world->GetComponent<ECS::TransformComponent>(entity);
        if (transform) transform->scale.z = scalarValue;
    }
    else if (propertyPath == "transform.rotation") {
        // vecValue contains Euler angles (radians) -- convert to Quaternion
        auto* transform = world->GetComponent<ECS::TransformComponent>(entity);
        if (transform) {
            transform->rotation = Math::Quaternion::FromEuler(vecValue);
        }
    }
    else if (propertyPath == "transform.rotation.x") {
        auto* transform = world->GetComponent<ECS::TransformComponent>(entity);
        if (transform) {
            Math::Vector3 euler = transform->rotation.ToEuler();
            euler.x = scalarValue;
            transform->rotation = Math::Quaternion::FromEuler(euler);
        }
    }
    else if (propertyPath == "transform.rotation.y") {
        auto* transform = world->GetComponent<ECS::TransformComponent>(entity);
        if (transform) {
            Math::Vector3 euler = transform->rotation.ToEuler();
            euler.y = scalarValue;
            transform->rotation = Math::Quaternion::FromEuler(euler);
        }
    }
    else if (propertyPath == "transform.rotation.z") {
        auto* transform = world->GetComponent<ECS::TransformComponent>(entity);
        if (transform) {
            Math::Vector3 euler = transform->rotation.ToEuler();
            euler.z = scalarValue;
            transform->rotation = Math::Quaternion::FromEuler(euler);
        }
    }
    // --- Material properties ---
    else if (propertyPath == "material.opacity") {
        auto* mat = world->GetComponent<ECS::MaterialComponent>(entity);
        if (mat) mat->opacity = scalarValue;
    }
    else if (propertyPath == "material.emissiveStrength") {
        auto* mat = world->GetComponent<ECS::MaterialComponent>(entity);
        if (mat) mat->emissiveStrength = scalarValue;
    }
    else if (propertyPath == "material.metallic") {
        auto* mat = world->GetComponent<ECS::MaterialComponent>(entity);
        if (mat) mat->metallic = scalarValue;
    }
    else if (propertyPath == "material.roughness") {
        auto* mat = world->GetComponent<ECS::MaterialComponent>(entity);
        if (mat) mat->roughness = scalarValue;
    }
    else if (propertyPath == "material.baseColor") {
        auto* mat = world->GetComponent<ECS::MaterialComponent>(entity);
        if (mat) mat->baseColor = vecValue;
    }
    else if (propertyPath == "material.emissiveColor") {
        auto* mat = world->GetComponent<ECS::MaterialComponent>(entity);
        if (mat) mat->emissiveColor = vecValue;
    }
    // --- Light properties ---
    else if (propertyPath == "light.intensity") {
        auto* light = world->GetComponent<ECS::LightComponent>(entity);
        if (light) light->intensity = scalarValue;
    }
    else if (propertyPath == "light.range") {
        auto* light = world->GetComponent<ECS::LightComponent>(entity);
        if (light) light->range = scalarValue;
    }
    else if (propertyPath == "light.color") {
        auto* light = world->GetComponent<ECS::LightComponent>(entity);
        if (light) light->color = vecValue;
    }
    else if (propertyPath == "light.innerConeAngle") {
        auto* light = world->GetComponent<ECS::LightComponent>(entity);
        if (light) light->innerConeAngle = scalarValue;
    }
    else if (propertyPath == "light.outerConeAngle") {
        auto* light = world->GetComponent<ECS::LightComponent>(entity);
        if (light) light->outerConeAngle = scalarValue;
    }
    // --- Sprite properties ---
    else if (propertyPath == "sprite.color" || propertyPath == "sprite.tint") {
        auto* sprite = world->GetComponent<ECS::Sprite2DComponent>(entity);
        if (sprite) sprite->tint = vecValue;
    }
    else if (propertyPath == "sprite.alpha") {
        auto* sprite = world->GetComponent<ECS::Sprite2DComponent>(entity);
        if (sprite) sprite->alpha = scalarValue;
    }
    else {
        ENJIN_LOG_WARN(Animation, "TimelineEditor: Unknown property path '%s'", propertyPath.c_str());
    }
}

f32 TimelineEditor::ReadPropertyFloat(ECS::World* world, ECS::Entity entity,
                                      const std::string& propertyPath) const {
    if (!world) return 0.0f;

    // --- Transform scalars ---
    if (propertyPath == "transform.position.x") {
        auto* t = world->GetComponent<ECS::TransformComponent>(entity);
        return t ? t->position.x : 0.0f;
    }
    if (propertyPath == "transform.position.y") {
        auto* t = world->GetComponent<ECS::TransformComponent>(entity);
        return t ? t->position.y : 0.0f;
    }
    if (propertyPath == "transform.position.z") {
        auto* t = world->GetComponent<ECS::TransformComponent>(entity);
        return t ? t->position.z : 0.0f;
    }
    if (propertyPath == "transform.scale.x") {
        auto* t = world->GetComponent<ECS::TransformComponent>(entity);
        return t ? t->scale.x : 1.0f;
    }
    if (propertyPath == "transform.scale.y") {
        auto* t = world->GetComponent<ECS::TransformComponent>(entity);
        return t ? t->scale.y : 1.0f;
    }
    if (propertyPath == "transform.scale.z") {
        auto* t = world->GetComponent<ECS::TransformComponent>(entity);
        return t ? t->scale.z : 1.0f;
    }
    if (propertyPath == "transform.rotation.x") {
        auto* t = world->GetComponent<ECS::TransformComponent>(entity);
        return t ? t->rotation.ToEuler().x : 0.0f;
    }
    if (propertyPath == "transform.rotation.y") {
        auto* t = world->GetComponent<ECS::TransformComponent>(entity);
        return t ? t->rotation.ToEuler().y : 0.0f;
    }
    if (propertyPath == "transform.rotation.z") {
        auto* t = world->GetComponent<ECS::TransformComponent>(entity);
        return t ? t->rotation.ToEuler().z : 0.0f;
    }
    // --- Material scalars ---
    if (propertyPath == "material.opacity") {
        auto* m = world->GetComponent<ECS::MaterialComponent>(entity);
        return m ? m->opacity : 1.0f;
    }
    if (propertyPath == "material.emissiveStrength") {
        auto* m = world->GetComponent<ECS::MaterialComponent>(entity);
        return m ? m->emissiveStrength : 0.0f;
    }
    if (propertyPath == "material.metallic") {
        auto* m = world->GetComponent<ECS::MaterialComponent>(entity);
        return m ? m->metallic : 0.0f;
    }
    if (propertyPath == "material.roughness") {
        auto* m = world->GetComponent<ECS::MaterialComponent>(entity);
        return m ? m->roughness : 0.5f;
    }
    // --- Light scalars ---
    if (propertyPath == "light.intensity") {
        auto* l = world->GetComponent<ECS::LightComponent>(entity);
        return l ? l->intensity : 1.0f;
    }
    if (propertyPath == "light.range") {
        auto* l = world->GetComponent<ECS::LightComponent>(entity);
        return l ? l->range : 10.0f;
    }
    if (propertyPath == "light.innerConeAngle") {
        auto* l = world->GetComponent<ECS::LightComponent>(entity);
        return l ? l->innerConeAngle : 12.5f;
    }
    if (propertyPath == "light.outerConeAngle") {
        auto* l = world->GetComponent<ECS::LightComponent>(entity);
        return l ? l->outerConeAngle : 17.5f;
    }
    // --- Sprite scalars ---
    if (propertyPath == "sprite.alpha") {
        auto* s = world->GetComponent<ECS::Sprite2DComponent>(entity);
        return s ? s->alpha : 1.0f;
    }

    ENJIN_LOG_WARN(Animation, "TimelineEditor: Cannot read float from '%s'", propertyPath.c_str());
    return 0.0f;
}

Math::Vector3 TimelineEditor::ReadPropertyVec3(ECS::World* world, ECS::Entity entity,
                                               const std::string& propertyPath) const {
    if (!world) return Math::Vector3(0.0f);

    // --- Transform vectors ---
    if (propertyPath == "transform.position") {
        auto* t = world->GetComponent<ECS::TransformComponent>(entity);
        return t ? t->position : Math::Vector3(0.0f);
    }
    if (propertyPath == "transform.scale") {
        auto* t = world->GetComponent<ECS::TransformComponent>(entity);
        return t ? t->scale : Math::Vector3(1.0f);
    }
    if (propertyPath == "transform.rotation") {
        auto* t = world->GetComponent<ECS::TransformComponent>(entity);
        return t ? t->rotation.ToEuler() : Math::Vector3(0.0f);
    }
    // --- Material vectors ---
    if (propertyPath == "material.baseColor") {
        auto* m = world->GetComponent<ECS::MaterialComponent>(entity);
        return m ? m->baseColor : Math::Vector3(1.0f);
    }
    if (propertyPath == "material.emissiveColor") {
        auto* m = world->GetComponent<ECS::MaterialComponent>(entity);
        return m ? m->emissiveColor : Math::Vector3(0.0f);
    }
    // --- Light vectors ---
    if (propertyPath == "light.color") {
        auto* l = world->GetComponent<ECS::LightComponent>(entity);
        return l ? l->color : Math::Vector3(1.0f);
    }
    // --- Sprite vectors ---
    if (propertyPath == "sprite.color" || propertyPath == "sprite.tint") {
        auto* s = world->GetComponent<ECS::Sprite2DComponent>(entity);
        return s ? s->tint : Math::Vector3(1.0f);
    }

    ENJIN_LOG_WARN(Animation, "TimelineEditor: Cannot read Vector3 from '%s'", propertyPath.c_str());
    return Math::Vector3(0.0f);
}

// ============================================================================
// Playback controls
// ============================================================================

void TimelineEditor::Play() {
    m_State.isPlaying = true;
}

void TimelineEditor::Pause() {
    m_State.isPlaying = false;
}

void TimelineEditor::Stop() {
    m_State.isPlaying = false;
    m_State.currentTime = 0.0f;
}

void TimelineEditor::SetTime(f32 time) {
    m_State.currentTime = std::clamp(time, 0.0f, m_State.duration);
}

void TimelineEditor::StepForward() {
    f32 frameTime = 1.0f / m_State.fps;
    m_State.currentTime = std::min(m_State.currentTime + frameTime, m_State.duration);
}

void TimelineEditor::StepBackward() {
    f32 frameTime = 1.0f / m_State.fps;
    m_State.currentTime = std::max(m_State.currentTime - frameTime, 0.0f);
}

void TimelineEditor::GoToStart() {
    m_State.currentTime = 0.0f;
}

void TimelineEditor::GoToEnd() {
    m_State.currentTime = m_State.duration;
}

void TimelineEditor::Update(f32 dt) {
    if (!m_State.isPlaying) return;

    m_State.currentTime += dt;

    if (m_State.currentTime >= m_State.duration) {
        if (m_State.loop) {
            m_State.currentTime = std::fmod(m_State.currentTime, m_State.duration);
        } else {
            m_State.currentTime = m_State.duration;
            m_State.isPlaying = false;
        }
    }
}

// ============================================================================
// Copy/paste
// ============================================================================

void TimelineEditor::CopySelectedKeyframes() {
    m_Clipboard.clear();

    if (m_State.selectedKeyframes.empty()) return;

    // Find the earliest selected keyframe time to use as relative origin
    f32 earliestTime = std::numeric_limits<f32>::max();
    for (const auto& [trackIdx, kfIdx] : m_State.selectedKeyframes) {
        // Search all layers for the track at this index
        for (const auto& layer : m_State.layers) {
            if (trackIdx >= 0 && trackIdx < static_cast<i32>(layer.tracks.size())) {
                const auto& track = layer.tracks[trackIdx];
                if (kfIdx >= 0 && kfIdx < static_cast<i32>(track.keyframes.size())) {
                    earliestTime = std::min(earliestTime, track.keyframes[kfIdx].time);
                }
            }
        }
    }

    // Copy keyframes relative to the earliest time
    for (const auto& [trackIdx, kfIdx] : m_State.selectedKeyframes) {
        for (const auto& layer : m_State.layers) {
            if (trackIdx >= 0 && trackIdx < static_cast<i32>(layer.tracks.size())) {
                const auto& track = layer.tracks[trackIdx];
                if (kfIdx >= 0 && kfIdx < static_cast<i32>(track.keyframes.size())) {
                    const auto& kf = track.keyframes[kfIdx];
                    KeyframeClip clip;
                    clip.relativeTime = kf.time - earliestTime;
                    clip.value = kf.value;
                    clip.vectorValue = kf.vectorValue;
                    clip.interpolation = kf.interpolation;
                    m_Clipboard.push_back(clip);
                }
            }
        }
    }
}

void TimelineEditor::PasteKeyframes(f32 time) {
    if (m_Clipboard.empty()) return;
    if (m_State.selectedLayerIndex < 0) return;
    if (m_State.selectedTrackIndex < 0) return;

    u32 layerIdx = static_cast<u32>(m_State.selectedLayerIndex);
    u32 trackIdx = static_cast<u32>(m_State.selectedTrackIndex);

    if (layerIdx >= static_cast<u32>(m_State.layers.size())) return;
    auto& tracks = m_State.layers[layerIdx].tracks;
    if (trackIdx >= static_cast<u32>(tracks.size())) return;

    auto& track = tracks[trackIdx];

    for (const auto& clip : m_Clipboard) {
        f32 pasteTime = time + clip.relativeTime;
        if (pasteTime < 0.0f || pasteTime > m_State.duration) continue;

        if (track.propertyType == AnimatedPropertyType::Vector3 ||
            track.propertyType == AnimatedPropertyType::Color) {
            AddKeyframeVec3(layerIdx, trackIdx, pasteTime, clip.vectorValue);
        } else {
            AddKeyframe(layerIdx, trackIdx, pasteTime, clip.value);
        }
    }
}

void TimelineEditor::DeleteSelectedKeyframes() {
    // Delete in reverse order to preserve indices
    // Sort by (trackIndex, keyframeIndex) descending
    auto sorted = m_State.selectedKeyframes;
    std::sort(sorted.begin(), sorted.end(),
        [](const std::pair<i32, i32>& a, const std::pair<i32, i32>& b) {
            if (a.first != b.first) return a.first > b.first;
            return a.second > b.second;
        });

    for (const auto& [trackIdx, kfIdx] : sorted) {
        // Find the track across all layers and delete the keyframe
        for (auto& layer : m_State.layers) {
            if (trackIdx >= 0 && trackIdx < static_cast<i32>(layer.tracks.size())) {
                auto& keyframes = layer.tracks[trackIdx].keyframes;
                if (kfIdx >= 0 && kfIdx < static_cast<i32>(keyframes.size())) {
                    keyframes.erase(keyframes.begin() + kfIdx);
                }
            }
        }
    }

    m_State.selectedKeyframes.clear();
    m_State.selectedKeyframeIndex = -1;
}

// ============================================================================
// Time/frame conversion utilities
// ============================================================================

f32 TimelineEditor::TimeToFrame(f32 time) const {
    return time * m_State.fps;
}

f32 TimelineEditor::FrameToTime(f32 frame) const {
    if (m_State.fps <= 0.0f) return 0.0f;
    return frame / m_State.fps;
}

f32 TimelineEditor::SnapToFrame(f32 time) const {
    if (m_State.fps <= 0.0f) return time;
    f32 frame = std::round(time * m_State.fps);
    return frame / m_State.fps;
}

} // namespace Animation
} // namespace Enjin
