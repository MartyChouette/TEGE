#include "Enjin/Renderer/CameraController.h"
#include "Enjin/Platform/Input.h"
#include "Enjin/Math/Math.h"
#include "Enjin/Math/Quaternion.h"

namespace Enjin {
namespace Renderer {

CameraController::CameraController(Camera* camera)
    : m_Camera(camera) {
    if (m_Camera) {
        SyncFromCamera();
    }
}

void CameraController::SyncFromCamera() {
    if (!m_Camera) return;

    // Extract yaw and pitch from the camera's forward vector.
    // Convention: yaw=0 looks along -Z, pitch=0 is horizontal,
    // positive pitch looks up, negative looks down.
    Math::Vector3 forward = m_Camera->GetForward();

    m_Pitch = Math::Degrees(Math::Asin(Math::Clamp(forward.y, -1.0f, 1.0f)));
    m_Yaw = Math::Degrees(Math::Atan2(forward.x, -forward.z));

    // Normalize yaw to [-180, 180] to avoid accumulation drift
    while (m_Yaw > 180.0f) m_Yaw -= 360.0f;
    while (m_Yaw < -180.0f) m_Yaw += 360.0f;

    m_Pitch = Math::Clamp(m_Pitch, -89.0f, 89.0f);
}

void CameraController::Update(f32 deltaTime) {
    if (!m_Camera || !m_Enabled) {
        // Clear our tracking flag but don't release mouse capture —
        // play mode may have captured it for the FPS controller
        m_MouseCapturedByUs = false;
        return;
    }

    switch (m_Mode) {
        case CameraMode::Fly:
            UpdateFlyMode(deltaTime);
            break;
        case CameraMode::Orbit:
            UpdateOrbitMode(deltaTime);
            break;
        case CameraMode::FirstPerson:
            UpdateFirstPersonMode(deltaTime);
            break;
    }
}

void CameraController::UpdateFlyMode(f32 deltaTime) {
    // Right mouse button OR middle mouse button to look around.
    // Mouse is captured while held — cursor hidden, free look active.
    bool wantLook = Input::IsMouseButtonDown(MouseButton::Right) ||
                    Input::IsMouseButtonDown(MouseButton::Middle);

    if (wantLook) {
        if (!m_MouseCapturedByUs) {
            Input::SetMouseCaptured(true);
            m_MouseCapturedByUs = true;
            SyncFromCamera();
            // Consume the first-frame mouse delta so the camera doesn't whip
            // to wherever the mouse was moving before the button was pressed
            Input::GetMouseDelta();
        } else {
            Math::Vector2 mouseDelta = Input::GetMouseDelta();
            m_Yaw += mouseDelta.x * m_LookSensitivity;
            m_Pitch -= mouseDelta.y * m_LookSensitivity;  // Inverted: drag up → look up
            m_Pitch = Math::Clamp(m_Pitch, -89.0f, 89.0f);
            ApplyRotation();
        }
    } else if (m_MouseCapturedByUs) {
        Input::SetMouseCaptured(false);
        m_MouseCapturedByUs = false;
    }

    // Gamepad right stick for camera look (always active, no button hold required)
    if (Input::IsGamepadConnected()) {
        Math::Vector2 rightStick = Input::GetGamepadRightStick();
        if (rightStick.x != 0.0f || rightStick.y != 0.0f) {
            m_Yaw += rightStick.x * m_LookSensitivity * 80.0f * deltaTime;
            m_Pitch += rightStick.y * m_LookSensitivity * 80.0f * deltaTime;
            m_Pitch = Math::Clamp(m_Pitch, -89.0f, 89.0f);
            ApplyRotation();
        }
    }

    // Movement - FPS-style: WASD on horizontal plane, Space/Ctrl for vertical
    f32 speed = m_MoveSpeed;
    if (Input::IsKeyDown(KeyCode::LeftShift) || Input::IsKeyDown(KeyCode::RightShift)) {
        speed *= m_SprintMultiplier;
    }
    // Gamepad sprint: left stick click
    if (Input::IsGamepadConnected() && Input::IsGamepadButtonDown(GamepadButton::LeftStick)) {
        speed *= m_SprintMultiplier;
    }

    Math::Vector3 movement(0.0f, 0.0f, 0.0f);

    // Derive forward/right from the view matrix (not m_Rotation which may be stale).
    // W/S move in the full look direction (including pitch) for true 3D fly.
    // A/D strafe perpendicular. Q/E move on world Y axis.
    Math::Matrix4 viewMat = m_Camera->GetViewMatrix();
    // View matrix rows are: right (row 0), up (row 1), -forward (row 2)
    Math::Vector3 right(viewMat.m[0], viewMat.m[4], viewMat.m[8]);
    Math::Vector3 forward(-viewMat.m[2], -viewMat.m[6], -viewMat.m[10]);

    Math::Vector3 worldUp(0.0f, 1.0f, 0.0f);

    if (Input::IsKeyDown(KeyCode::W)) {
        movement = movement + forward;
    }
    if (Input::IsKeyDown(KeyCode::S)) {
        movement = movement - forward;
    }
    if (Input::IsKeyDown(KeyCode::A)) {
        movement = movement - right;
    }
    if (Input::IsKeyDown(KeyCode::D)) {
        movement = movement + right;
    }
    if (Input::IsKeyDown(KeyCode::E)) {
        movement = movement + worldUp;
    }
    if (Input::IsKeyDown(KeyCode::Q)) {
        movement = movement - worldUp;
    }

    // Gamepad left stick for movement
    if (Input::IsGamepadConnected()) {
        Math::Vector2 leftStick = Input::GetGamepadLeftStick();
        if (leftStick.x != 0.0f || leftStick.y != 0.0f) {
            movement = movement + right * leftStick.x;
            movement = movement - forward * leftStick.y;  // Y inverted: up = forward
        }
        // Triggers for vertical movement (RT = up, LT = down)
        f32 rt = Input::GetGamepadRightTrigger();
        f32 lt = Input::GetGamepadLeftTrigger();
        if (rt > 0.1f) movement = movement + worldUp * rt;
        if (lt > 0.1f) movement = movement - worldUp * lt;
    }

    // Normalize and apply movement
    f32 length = movement.Length();
    if (length > 0.001f) {
        movement = movement * (1.0f / length);
        Math::Vector3 newPos = m_Camera->GetPosition() + movement * speed * deltaTime;
        m_Camera->SetPosition(newPos);
    }

    // Scroll to adjust speed (or orbit distance when MMB orbiting)
    Math::Vector2 scroll = Input::GetScrollDelta();
    if (scroll.y != 0.0f) {
        m_MoveSpeed *= (1.0f + scroll.y * 0.1f);
        m_MoveSpeed = Math::Clamp(m_MoveSpeed, 0.1f, 100.0f);
    }
    // Gamepad: D-pad up/down to adjust speed
    if (Input::IsGamepadConnected()) {
        if (Input::IsGamepadButtonPressed(GamepadButton::DPadUp)) {
            m_MoveSpeed *= 1.5f;
            m_MoveSpeed = Math::Clamp(m_MoveSpeed, 0.1f, 100.0f);
        }
        if (Input::IsGamepadButtonPressed(GamepadButton::DPadDown)) {
            m_MoveSpeed *= 0.67f;
            m_MoveSpeed = Math::Clamp(m_MoveSpeed, 0.1f, 100.0f);
        }
    }
}

void CameraController::UpdateOrbitMode(f32 deltaTime) {
    (void)deltaTime;

    // Hold right mouse button to orbit
    bool rightMouseDown = Input::IsMouseButtonDown(MouseButton::Right);

    if (rightMouseDown) {
        if (!m_MouseCapturedByUs) {
            Input::SetMouseCaptured(true);
            m_MouseCapturedByUs = true;
            SyncFromCamera();
            Input::GetMouseDelta(); // consume first-frame delta
        } else {
            Math::Vector2 mouseDelta = Input::GetMouseDelta();
            m_Yaw += mouseDelta.x * m_LookSensitivity;
            m_Pitch -= mouseDelta.y * m_LookSensitivity;  // Inverted: drag up → look up
            m_Pitch = Math::Clamp(m_Pitch, -89.0f, 89.0f);
        }
    } else if (m_MouseCapturedByUs) {
        Input::SetMouseCaptured(false);
        m_MouseCapturedByUs = false;
    }

    // Middle mouse to pan
    bool middleMouseDown = Input::IsMouseButtonDown(MouseButton::Middle);
    if (middleMouseDown) {
        Math::Vector2 mouseDelta = Input::GetMouseDelta();
        Math::Vector3 right = m_Camera->GetRight();
        Math::Vector3 up = m_Camera->GetUp();
        f32 panSpeed = m_OrbitDistance * 0.002f;
        m_OrbitTarget = m_OrbitTarget - right * mouseDelta.x * panSpeed;
        m_OrbitTarget = m_OrbitTarget + up * mouseDelta.y * panSpeed;
    }

    // Scroll to zoom
    Math::Vector2 scroll = Input::GetScrollDelta();
    if (scroll.y != 0.0f) {
        m_OrbitDistance *= (1.0f - scroll.y * 0.1f);
        m_OrbitDistance = Math::Clamp(m_OrbitDistance, m_MinOrbitDistance, m_MaxOrbitDistance);
    }

    // Calculate camera position from orbit parameters.
    // Convention: yaw=0 looks along -Z, so the camera offset is along +Z.
    // The camera sits BEHIND the target relative to its forward direction.
    f32 yawRad = Math::Radians(m_Yaw);
    f32 pitchRad = Math::Radians(m_Pitch);

    // Spherical to cartesian: camera is at (target + offset), looking at target.
    // Forward = (sin(yaw)*cos(pitch), sin(pitch), -cos(yaw)*cos(pitch))
    // Camera offset = -forward * distance
    Math::Vector3 offset;
    offset.x = -Math::Sin(yawRad) * Math::Cos(pitchRad) * m_OrbitDistance;
    offset.y = -Math::Sin(pitchRad) * m_OrbitDistance;
    offset.z = Math::Cos(yawRad) * Math::Cos(pitchRad) * m_OrbitDistance;

    Math::Vector3 cameraPos = m_OrbitTarget + offset;
    m_Camera->SetPosition(cameraPos);

    // Look at target — use world up unless nearly vertical
    Math::Vector3 up(0.0f, 1.0f, 0.0f);
    m_Camera->SetLookAt(cameraPos, m_OrbitTarget, up);
}

void CameraController::UpdateFirstPersonMode(f32 deltaTime) {
    // Same as fly but with gravity/ground constraint (simplified for now)
    UpdateFlyMode(deltaTime);

    // Constrain to ground plane (y = fixed height above grid)
    Math::Vector3 pos = m_Camera->GetPosition();
    if (pos.y < 1.7f) {
        pos.y = 1.7f; // Eye height - ensure we're above grid level
    }
    m_Camera->SetPosition(pos);
}

void CameraController::ApplyRotation() {
    // Build forward vector from yaw/pitch, then use LookAt to set camera rotation.
    // This avoids quaternion multiplication order issues and gimbal artifacts.
    f32 yawRad = Math::Radians(m_Yaw);
    f32 pitchRad = Math::Radians(m_Pitch);

    Math::Vector3 forward;
    forward.x = Math::Sin(yawRad) * Math::Cos(pitchRad);
    forward.y = Math::Sin(pitchRad);
    forward.z = -Math::Cos(yawRad) * Math::Cos(pitchRad);

    Math::Vector3 pos = m_Camera->GetPosition();
    Math::Vector3 target = pos + forward;
    m_Camera->SetLookAt(pos, target, Math::Vector3(0.0f, 1.0f, 0.0f));
}

void CameraController::SetViewPreset(ViewPreset preset) {
    if (!m_Camera) return;

    m_ViewPreset = preset;
    Math::Vector3 target = m_OrbitTarget;
    f32 distance = m_OrbitDistance;

    switch (preset) {
        case ViewPreset::Perspective:
            // Keep current position, switch to perspective
            m_IsOrthographic = false;
            m_Camera->SetPerspective(45.0f, 16.0f / 9.0f, 0.1f, 1000.0f);
            return;

        // Presets use the orbit convention: camera is at (target - forward * distance).
        // yaw=0 forward is -Z, so camera is at +Z. Presets set yaw/pitch and let
        // the orbit update compute the position.
        case ViewPreset::Top:
            m_Yaw = 0.0f;
            m_Pitch = 89.0f;   // Looking straight down (camera above, pitch up = looking down at target)
            break;

        case ViewPreset::Bottom:
            m_Yaw = 0.0f;
            m_Pitch = -89.0f;  // Looking straight up
            break;

        case ViewPreset::Front:
            m_Yaw = 0.0f;      // Forward is -Z, camera at +Z looking at target
            m_Pitch = 0.0f;
            break;

        case ViewPreset::Back:
            m_Yaw = 180.0f;    // Camera at -Z looking at target
            m_Pitch = 0.0f;
            break;

        case ViewPreset::Right:
            m_Yaw = -90.0f;    // Camera at +X looking at target
            m_Pitch = 0.0f;
            break;

        case ViewPreset::Left:
            m_Yaw = 90.0f;     // Camera at -X looking at target
            m_Pitch = 0.0f;
            break;
    }

    // Use the orbit math to compute camera position from yaw/pitch/distance
    {
        f32 yawRad = Math::Radians(m_Yaw);
        f32 pitchRad = Math::Radians(m_Pitch);
        Math::Vector3 offset;
        offset.x = -Math::Sin(yawRad) * Math::Cos(pitchRad) * distance;
        offset.y = -Math::Sin(pitchRad) * distance;
        offset.z = Math::Cos(yawRad) * Math::Cos(pitchRad) * distance;
        m_Camera->SetPosition(target + offset);
        m_Camera->SetLookAt(target + offset, target, Math::Vector3(0.0f, 1.0f, 0.0f));
    }

    // Set orthographic for preset views (except perspective)
    if (preset != ViewPreset::Perspective) {
        SetOrthographic(true);
    }
}

void CameraController::SetOrthographic(bool ortho) {
    if (!m_Camera) return;

    m_IsOrthographic = ortho;
    if (ortho) {
        f32 aspect = 16.0f / 9.0f;
        f32 halfHeight = m_OrthoSize * 0.5f;
        f32 halfWidth = halfHeight * aspect;
        m_Camera->SetOrthographic(-halfWidth, halfWidth, -halfHeight, halfHeight, 0.1f, 1000.0f);
    } else {
        m_Camera->SetPerspective(45.0f, 16.0f / 9.0f, 0.1f, 1000.0f);
    }
}

} // namespace Renderer
} // namespace Enjin
