#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Matrix.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Tween.h"
#include "Enjin/Editor/EditorSettings.h"
#include <imgui.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

namespace Enjin {
namespace Editor {

// ============================================================================
// Flash-Style Timeline Editor
// ============================================================================
// A frame-based timeline panel inspired by Flash/Animate's timeline.
// Features layers, keyframes, tweens, frame labels, and scrubbing.
// Extends the existing Animation/Sequencer system.

// --- Frame types ---

enum class FlashFrameType : u8 {
    Empty,          // Blank frame
    Keyframe,       // Contains new content
    StaticFrame,    // Extends previous keyframe content
    MotionTween,    // Interpolated motion between keyframes
    ShapeTween      // Interpolated shape morph (not fully supported)
};

// --- Onion skinning settings ---

struct OnionSkinSettings {
    bool enabled = false;
    i32 framesBefore = 2;
    i32 framesAfter = 1;
    f32 opacity = 0.3f;
    f32 opacityFalloff = 0.5f;
    Math::Vector3 beforeTint{1.0f, 0.4f, 0.4f};  // Red
    Math::Vector3 afterTint{0.4f, 1.0f, 0.4f};    // Green
};

// --- Onion skin ghost (render data for a single ghost frame) ---

struct OnionSkinGhost {
    ECS::Entity entity;
    Math::Vector3 position;
    Math::Vector3 rotation;
    Math::Vector3 scale;
    f32 alpha;
    Math::Vector3 tint;
    f32 ghostOpacity;

    // 3D skeletal onion skin: pre-computed skinning matrices for this ghost frame.
    // Empty for non-skinned (2D sprite) ghosts.
    std::vector<Math::Matrix4> skinningMatrices;
};

// --- Keyframe data ---

struct FlashKeyframe {
    u32 frameIndex = 0;
    FlashFrameType type = FlashFrameType::Keyframe;

    // Transform at this keyframe
    Math::Vector3 position;
    Math::Vector3 rotation;
    Math::Vector3 scale = Math::Vector3(1, 1, 1);
    f32 alpha = 1.0f;
    bool visible = true;

    // Tween settings (applies from this keyframe to the next)
    bool tweenMotion = false;
    ECS::EasingType tweenEasing = ECS::EasingType::Linear;  // 25 easing functions

    // Frame label (like Flash frame labels for gotoAndPlay)
    std::string label;

    // Frame script (AngelScript snippet to execute at this frame)
    std::string script;

    // Sound to play at this frame
    std::string soundPath;
};

// --- Timeline layer ---

struct FlashTimelineLayer {
    std::string name = "Layer 1";
    bool visible = true;
    bool locked = false;
    bool isGuide = false;       // Guide layer (not rendered at runtime)
    bool isMask = false;        // Mask layer

    ECS::Entity entity = 0;     // The entity this layer controls

    // Keyframes sorted by frameIndex
    std::vector<FlashKeyframe> keyframes;

    // Helper: get the active keyframe at a given frame
    const FlashKeyframe* GetKeyframeAt(u32 frame) const;
    FlashKeyframe* GetKeyframeAt(u32 frame);

    // Helper: get or create keyframe
    FlashKeyframe& GetOrCreateKeyframe(u32 frame);

    // Helper: remove keyframe
    void RemoveKeyframe(u32 frame);

    // Helper: get interpolated transform at a frame (handles tweens)
    void GetInterpolatedTransform(u32 frame, Math::Vector3& pos,
                                   Math::Vector3& rot, Math::Vector3& scl,
                                   f32& alpha) const;
};

// --- Timeline document ---

struct FlashTimelineData {
    std::string name = "Timeline";
    u32 totalFrames = 60;
    f32 frameRate = 24.0f;

    std::vector<FlashTimelineLayer> layers;

    // Stage/canvas size (for 2D layout)
    f32 stageWidth = 550.0f;
    f32 stageHeight = 400.0f;
    u32 backgroundColor = 0xFFFFFF;

    // Symbol library — maps symbol names to prefab paths
    std::unordered_map<std::string, std::string> symbolLibrary;

    // Playback state
    u32 currentFrame = 0;
    bool isPlaying = false;
    bool loop = true;

    // Helpers
    void AddLayer(const std::string& name, ECS::Entity entity = 0);
    void RemoveLayer(u32 index);
    void MoveLayer(u32 from, u32 to);
    void InsertFrames(u32 atFrame, u32 count);
    void DeleteFrames(u32 atFrame, u32 count);
};

// ============================================================================
// Flash Timeline Editor Widget
// ============================================================================

class ENJIN_API FlashTimelineEditor {
public:
    FlashTimelineEditor() = default;

    // Set the timeline data to edit
    void SetTimeline(FlashTimelineData* timeline) { m_Timeline = timeline; }
    FlashTimelineData* GetTimeline() const { return m_Timeline; }

    // Main render function
    void Render(ECS::World* world, const EditorSettings& settings);

    // Playback
    void Play();
    void Pause();
    void Stop();
    void StepForward();
    void StepBackward();
    void GotoFrame(u32 frame);

    // Update (advances playback if playing)
    void Update(f32 deltaTime);

    // Apply current frame's transforms to entities
    void ApplyFrame(ECS::World* world);

    // Convert to/from engine TimelineComponent
    void ConvertToTimeline(ECS::World* world);

    // Import from SWF sprite data
    void ImportFromSWFSprite(const std::string& swfPath);

    // Onion skinning — compute ghost frames for rendering
    std::vector<OnionSkinGhost> ComputeOnionSkinGhosts() const;
    const OnionSkinSettings& GetOnionSkinSettings() const { return m_OnionSkin; }

    // Record mode — capture entity transform as keyframe
    void CaptureKeyframe(ECS::World* world, ECS::Entity entity);
    bool IsRecordMode() const { return m_RecordMode; }
    void SetRecordMode(bool record) { m_RecordMode = record; }

private:
    FlashTimelineData* m_Timeline = nullptr;

    // Editor state
    u32 m_SelectedLayer = 0;
    u32 m_SelectedFrame = 0;
    i32 m_DragFrame = -1;
    bool m_IsDraggingPlayhead = false;
    f32 m_ScrollX = 0;        // Horizontal scroll in the frame grid
    f32 m_ZoomLevel = 1.0f;   // Frame cell width multiplier

    // Playback timer
    f32 m_PlaybackTimer = 0;

    // Onion skinning
    OnionSkinSettings m_OnionSkin;

    // Record mode — captures keyframes on gizmo drag end
    bool m_RecordMode = false;
    f32 m_RecordBlinkTimer = 0.0f;

    // Rendering sub-functions
    void DrawToolbar();
    void DrawLayerList();
    void DrawFrameGrid();
    void DrawPlayhead();
    void DrawKeyframeInspector();
    void DrawSymbolLibrary(ECS::World* world);

    // Frame grid helpers
    f32 FrameToX(u32 frame) const;
    u32 XToFrame(f32 x) const;
    ImVec4 GetFrameColor(FlashFrameType type) const;
};

} // namespace Editor
} // namespace Enjin
