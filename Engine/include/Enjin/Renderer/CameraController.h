#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Renderer/Camera.h"
#include "Enjin/Math/Vector.h"

namespace Enjin {
namespace Renderer {

// Camera controller mode
enum class CameraMode {
    Fly,        // Free fly camera (WASD + mouse)
    Orbit,      // Orbit around a target point
    FirstPerson // First-person style
};

// View presets for quick camera positioning
enum class ViewPreset {
    Perspective,  // Free perspective view
    Top,          // Looking down (-Y)
    Bottom,       // Looking up (+Y)
    Front,        // Looking at -Z
    Back,         // Looking at +Z
    Right,        // Looking at -X
    Left          // Looking at +X
};

// Fly camera controller - uses Input system
class ENJIN_API CameraController {
public:
    CameraController(Camera* camera = nullptr);
    ~CameraController() = default;

    void SetCamera(Camera* camera) { m_Camera = camera; SyncFromCamera(); }
    Camera* GetCamera() const { return m_Camera; }

    // Sync yaw/pitch from current camera orientation (call after setting camera position/rotation externally)
    void SyncFromCamera();

    void Update(f32 deltaTime);

    // Settings
    void SetMode(CameraMode mode) { m_Mode = mode; }
    CameraMode GetMode() const { return m_Mode; }

    void SetMoveSpeed(f32 speed) { m_MoveSpeed = speed; }
    f32 GetMoveSpeed() const { return m_MoveSpeed; }

    void SetLookSensitivity(f32 sensitivity) { m_LookSensitivity = sensitivity; }
    f32 GetLookSensitivity() const { return m_LookSensitivity; }

    void SetSprintMultiplier(f32 multiplier) { m_SprintMultiplier = multiplier; }
    f32 GetSprintMultiplier() const { return m_SprintMultiplier; }

    // Orbit mode settings
    void SetOrbitTarget(const Math::Vector3& target) { m_OrbitTarget = target; }
    Math::Vector3 GetOrbitTarget() const { return m_OrbitTarget; }

    void SetOrbitDistance(f32 distance) { m_OrbitDistance = distance; }
    f32 GetOrbitDistance() const { return m_OrbitDistance; }

    // Enable/disable camera control (useful when ImGui wants mouse)
    void SetEnabled(bool enabled) { m_Enabled = enabled; }
    bool IsEnabled() const { return m_Enabled; }

    // Viewport input state from the host UI. pointerOver gates capture START
    // and scroll (so RMB-drag / scroll over other panels doesn't drive the
    // camera); focused additionally allows keyboard movement. An active
    // capture always keeps control until the button is released. Defaults are
    // true so standalone (non-editor) hosts keep the old always-on behavior.
    void SetViewportInputState(bool pointerOver, bool focused) {
        m_PointerOverViewport = pointerOver;
        m_ViewportFocused = focused;
    }

    // Check if camera controller currently has mouse captured (RMB held)
    bool IsMouseCaptured() const { return m_MouseCapturedByUs; }

    // Get current angles (for UI display)
    f32 GetYaw() const { return m_Yaw; }
    f32 GetPitch() const { return m_Pitch; }

    // View presets
    void SetViewPreset(ViewPreset preset);
    ViewPreset GetViewPreset() const { return m_ViewPreset; }

    // Orthographic mode
    void SetOrthographic(bool ortho);
    bool IsOrthographic() const { return m_IsOrthographic; }
    void SetOrthoSize(f32 size) { m_OrthoSize = size; }
    f32 GetOrthoSize() const { return m_OrthoSize; }

private:
    void UpdateFlyMode(f32 deltaTime);
    void UpdateOrbitMode(f32 deltaTime);
    void UpdateFirstPersonMode(f32 deltaTime);
    void ApplyRotation();

    Camera* m_Camera = nullptr;
    CameraMode m_Mode = CameraMode::Fly;
    bool m_Enabled = true;

    // Movement
    f32 m_MoveSpeed = 5.0f;
    f32 m_SprintMultiplier = 2.5f;
    f32 m_LookSensitivity = 0.1f;

    // Feel: frame-rate-independent smoothing. Velocity ramps toward the input
    // direction instead of snapping (instant start/stop felt stiff); the look
    // filter kills per-frame sensor jitter with ~1 frame of lag.
    f32 m_MoveAccel = 12.0f;        // 1/s — ~150ms to reach speed
    f32 m_MoveDecel = 16.0f;        // 1/s — slightly quicker glide-to-stop
    f32 m_LookSmoothTime = 0.008f;  // seconds; 0 = raw deltas
    Math::Vector3 m_Velocity = Math::Vector3(0.0f, 0.0f, 0.0f);
    Math::Vector2 m_SmoothedLook = Math::Vector2(0.0f, 0.0f);

    // Viewport input gating (see SetViewportInputState)
    bool m_PointerOverViewport = true;
    bool m_ViewportFocused = true;

    // Rotation (euler angles in degrees)
    f32 m_Yaw = -90.0f;   // Looking towards -Z by default
    f32 m_Pitch = 0.0f;

    // Orbit mode
    Math::Vector3 m_OrbitTarget = Math::Vector3(0.0f, 0.0f, 0.0f);
    f32 m_OrbitDistance = 5.0f;
    f32 m_MinOrbitDistance = 1.0f;
    f32 m_MaxOrbitDistance = 100.0f;

    // State
    bool m_MouseCapturedByUs = false;

    // View preset and orthographic
    ViewPreset m_ViewPreset = ViewPreset::Perspective;
    bool m_IsOrthographic = false;
    f32 m_OrthoSize = 10.0f;  // Orthographic view size
};

} // namespace Renderer
} // namespace Enjin
