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
