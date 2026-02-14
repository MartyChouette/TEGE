#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/ECS/World.h"
#include "Enjin/Animation/Timeline.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

namespace Enjin {
namespace Animation {

// Interpolation mode for keyframe curves
enum class KeyframeInterpolation : u8 {
    Constant,       // Hold value until next keyframe (step)
    Linear,         // Linear interpolation
    Bezier,         // Cubic bezier with control handles
    CatmullRom      // Smooth Catmull-Rom spline
};

// Extended keyframe with bezier handles for curve editing
struct EditorKeyframe {
    f32 time = 0.0f;
    f32 value = 0.0f;          // Scalar value (for non-vector properties)
    Math::Vector3 vectorValue;  // For position/rotation/scale
    KeyframeInterpolation interpolation = KeyframeInterpolation::Linear;

    // Bezier tangent handles (in/out)
    f32 tangentInTime = -0.1f;
    f32 tangentInValue = 0.0f;
    f32 tangentOutTime = 0.1f;
    f32 tangentOutValue = 0.0f;
    bool tangentsBroken = false; // If true, in/out tangents are independent
};

// Property types that can be animated
enum class AnimatedPropertyType : u8 {
    Float,
    Vector3,
    Color,
    Bool
};

// A track in the editor timeline
struct EditorTrack {
    std::string name;                   // Display name
    std::string propertyPath;           // "transform.position", "material.opacity", etc.
    ECS::Entity targetEntity = 0;
    AnimatedPropertyType propertyType = AnimatedPropertyType::Float;
    std::vector<EditorKeyframe> keyframes;
    bool locked = false;
    bool visible = true;
    bool solo = false;
    Math::Vector3 curveColor = {1.0f, 1.0f, 1.0f};  // Display color in curve editor
};

// Timeline layer (like Flash layers -- groups of tracks)
struct TimelineLayer {
    std::string name = "Layer 1";
    std::vector<EditorTrack> tracks;
    bool locked = false;
    bool visible = true;
    bool expanded = true;
};

// Onion-skinning settings (ghost frames before/after)
struct OnionSkinSettings {
    bool enabled = false;
    u32 framesBefore = 3;
    u32 framesAfter = 2;
    f32 opacityBefore = 0.3f;
    f32 opacityAfter = 0.2f;
    Math::Vector3 colorBefore = {0.3f, 0.3f, 0.8f};
    Math::Vector3 colorAfter = {0.8f, 0.3f, 0.3f};
};

// Editor state for the timeline
struct TimelineEditorState {
    std::vector<TimelineLayer> layers;
    f32 duration = 5.0f;            // Total timeline length in seconds
    f32 currentTime = 0.0f;
    f32 fps = 30.0f;                // Frame rate for frame display
    f32 zoom = 1.0f;                // Horizontal zoom level
    f32 scrollX = 0.0f;             // Horizontal scroll position
    f32 scrollY = 0.0f;             // Vertical scroll position
    bool isPlaying = false;
    bool loop = true;
    bool showCurves = false;        // Show curve editor instead of dopesheet
    bool snapToFrames = true;       // Snap keyframes to frame boundaries
    OnionSkinSettings onionSkin;

    // Selection state
    i32 selectedLayerIndex = -1;
    i32 selectedTrackIndex = -1;
    i32 selectedKeyframeIndex = -1;
    std::vector<std::pair<i32, i32>> selectedKeyframes; // (trackIndex, keyframeIndex) pairs

    // Drag state
    bool isDraggingKeyframe = false;
    bool isDraggingPlayhead = false;
    f32 dragStartTime = 0.0f;
    f32 dragStartValue = 0.0f;
};

// The timeline editor -- manages the authoring workflow
class ENJIN_API TimelineEditor {
public:
    TimelineEditor() = default;
    ~TimelineEditor() = default;

    // Create a new empty timeline for an entity
    void CreateTimeline(ECS::Entity entity, f32 duration = 5.0f);

    // Load from/save to TimelineComponent
    void LoadFromComponent(const TimelineComponent& component);
    TimelineComponent SaveToComponent() const;

    // Add/remove layers
    void AddLayer(const std::string& name = "New Layer");
    void RemoveLayer(u32 index);
    void MoveLayer(u32 fromIndex, u32 toIndex);

    // Add tracks
    void AddTrack(u32 layerIndex, const std::string& propertyPath,
                  AnimatedPropertyType type, ECS::Entity entity);
    void RemoveTrack(u32 layerIndex, u32 trackIndex);

    // Keyframe operations
    void AddKeyframe(u32 layerIndex, u32 trackIndex, f32 time, f32 value);
    void AddKeyframeVec3(u32 layerIndex, u32 trackIndex, f32 time, const Math::Vector3& value);
    void RemoveKeyframe(u32 layerIndex, u32 trackIndex, u32 keyframeIndex);
    void MoveKeyframe(u32 layerIndex, u32 trackIndex, u32 keyframeIndex, f32 newTime);

    // Auto-key: record current entity property values at current time
    void AutoKey(ECS::World* world);

    // Evaluate the timeline at a given time (applies to world entities)
    void Evaluate(ECS::World* world, f32 time);

    // Playback
    void Play();
    void Pause();
    void Stop();
    void SetTime(f32 time);
    void StepForward();
    void StepBackward();
    void GoToStart();
    void GoToEnd();

    // Update (advance playback)
    void Update(f32 dt);

    // Curve evaluation
    f32 EvaluateCurve(const EditorTrack& track, f32 time) const;
    Math::Vector3 EvaluateCurveVec3(const EditorTrack& track, f32 time) const;

    // Access
    TimelineEditorState& GetState() { return m_State; }
    const TimelineEditorState& GetState() const { return m_State; }
    bool IsOpen() const { return m_IsOpen; }
    void SetOpen(bool open) { m_IsOpen = open; }

    // Copy/paste keyframes
    void CopySelectedKeyframes();
    void PasteKeyframes(f32 time);
    void DeleteSelectedKeyframes();

    // Utility
    f32 TimeToFrame(f32 time) const;
    f32 FrameToTime(f32 frame) const;
    f32 SnapToFrame(f32 time) const;

private:
    TimelineEditorState m_State;
    bool m_IsOpen = false;
    ECS::Entity m_TargetEntity = 0;

    // Clipboard for copy/paste
    struct KeyframeClip {
        f32 relativeTime;
        f32 value;
        Math::Vector3 vectorValue;
        KeyframeInterpolation interpolation;
    };
    std::vector<KeyframeClip> m_Clipboard;

    // Bezier curve evaluation helper
    f32 EvaluateBezier(f32 t, f32 p0, f32 p1, f32 p2, f32 p3) const;
    f32 EvaluateCatmullRom(f32 t, f32 p0, f32 p1, f32 p2, f32 p3) const;

    // Apply a value to an entity property
    void ApplyPropertyValue(ECS::World* world, ECS::Entity entity,
                           const std::string& propertyPath,
                           AnimatedPropertyType type,
                           f32 scalarValue, const Math::Vector3& vecValue);

    // Read current property value from entity
    f32 ReadPropertyFloat(ECS::World* world, ECS::Entity entity,
                          const std::string& propertyPath) const;
    Math::Vector3 ReadPropertyVec3(ECS::World* world, ECS::Entity entity,
                                    const std::string& propertyPath) const;
};

} // namespace Animation
} // namespace Enjin
