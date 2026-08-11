#include "Enjin/Renderer/CameraController.h"
#include "Enjin/Platform/Input.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Math/Math.h"
#include "Enjin/Math/Quaternion.h"
#include <cmath>

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
        // Drop smoothed state so re-enabling doesn't glide on stale velocity
        m_Velocity = Math::Vector3(0.0f, 0.0f, 0.0f);
        m_SmoothedLook = Math::Vector2(0.0f, 0.0f);
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
    // Capture may only START over the viewport (RMB-dragging in the
    // Inspector used to rotate the camera); once captured it persists
    // until release regardless of where the hidden cursor sits.
    // RMB/MMB owns looking; WASD ONLY translates and must never disturb the
    // view direction. (An experiment auto-engaging look on WASD re-synced
    // orientation on every key tap and nudged the view — reverted per Marty,
    // 2026-08-07: "wasd should just move the cam location".)
    bool wantLook = (Input::IsMouseButtonDown(MouseButton::Right) ||
                     Input::IsMouseButtonDown(MouseButton::Middle)) &&
                    (m_MouseCapturedByUs || m_PointerOverViewport);

    if (wantLook) {
        if (!m_MouseCapturedByUs) {
            Input::SetMouseCaptured(true);
            m_MouseCapturedByUs = true;
            SyncFromCamera();
            // Input::SetMouseCaptured resets the delta origin, so the frame
            // after capture reads a clean 0. NO warmup frame: eating a frame
            // of look right as movement starts was the visible hitch when
            // RMB+WASD land together (UT-fly feel wants zero dead frames);
            // the 300px spike clamp below still guards capture-warp glitches.
            m_LookWarmupFrames = 0;
            m_SmoothedLook = Math::Vector2(0.0f, 0.0f);
        } else if (m_LookWarmupFrames > 0) {
            --m_LookWarmupFrames;
        } else {
            Math::Vector2 mouseDelta = Input::GetMouseDelta();
            // Spike clamp: a legitimate flick is well under this per frame; a
            // larger delta is a capture/warp artifact from some other system
            // toggling the cursor. Drop it rather than whip the view.
            if (mouseDelta.Length() > 300.0f) {
                mouseDelta = Math::Vector2(0.0f, 0.0f);
            }
            // Light exponential filter: kills per-frame sensor jitter with
            // about one frame of lag, and gives release a soft landing.
            if (m_LookSmoothTime > 0.0f) {
                f32 a = 1.0f - std::exp(-deltaTime / m_LookSmoothTime);
                m_SmoothedLook.x += (mouseDelta.x - m_SmoothedLook.x) * a;
                m_SmoothedLook.y += (mouseDelta.y - m_SmoothedLook.y) * a;
            } else {
                m_SmoothedLook = mouseDelta;
            }
            m_Yaw += m_SmoothedLook.x * m_LookSensitivity;
            m_Pitch -= m_SmoothedLook.y * m_LookSensitivity;  // Inverted: drag up → look up
            m_Pitch = Math::Clamp(m_Pitch, -89.0f, 89.0f);
            ApplyRotation();
        }
    } else if (m_MouseCapturedByUs) {
        Input::SetMouseCaptured(false);
        m_MouseCapturedByUs = false;
    }

    // Keyboard movement follows pointer-over/focus (or an active look-drag);
    // gamepad input is gated only by m_Enabled.
    bool keyboardAllowed = m_MouseCapturedByUs || m_PointerOverViewport || m_ViewportFocused;

    // Radial deadzone for gamepad sticks. Without this, a connected controller's normal stick
    // drift (commonly 0.05-0.15) feeds the fly camera every frame with no input, so the editor
    // view slowly rotates or sinks on its own. Applies to both sticks below.
    constexpr f32 kGamepadDeadzone = 0.2f;

    // Gamepad right stick for camera look (always active, no button hold required)
    if (Input::IsGamepadConnected()) {
        Math::Vector2 rightStick = Input::GetGamepadRightStick();
        if (rightStick.Length() > kGamepadDeadzone) {
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

    if (keyboardAllowed) {
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
    }

    // Gamepad left stick for movement
    if (Input::IsGamepadConnected()) {
        Math::Vector2 leftStick = Input::GetGamepadLeftStick();
        if (leftStick.Length() > kGamepadDeadzone) {
            movement = movement + right * leftStick.x;
            movement = movement - forward * leftStick.y;  // Y inverted: up = forward
        }
        // Triggers for vertical movement (RT = up, LT = down). Higher deadzone than the 0.1 before,
        // since a resting/creeping trigger otherwise pushes the camera straight down.
        f32 rt = Input::GetGamepadRightTrigger();
        f32 lt = Input::GetGamepadLeftTrigger();
        if (rt > kGamepadDeadzone) movement = movement + worldUp * rt;
        if (lt > kGamepadDeadzone) movement = movement - worldUp * lt;

        // Field-diagnosable breadcrumb: the "camera moves on its own" class of
        // bug has now happened twice (stick drift, then a phantom trigger from
        // a bad axis rest value). Log ONCE per session when gamepad input
        // first drives the fly camera, so the log names the culprit device
        // and axis values instead of leaving it to guesswork.
        static bool s_LoggedFirstGamepadMove = false;
        if (!s_LoggedFirstGamepadMove &&
            (leftStick.Length() > kGamepadDeadzone || rt > kGamepadDeadzone || lt > kGamepadDeadzone)) {
            s_LoggedFirstGamepadMove = true;
            ENJIN_LOG_INFO(Editor, "Fly camera moved by gamepad '%s': stick=(%.2f,%.2f) LT=%.2f RT=%.2f",
                           Input::GetGamepadName(), leftStick.x, leftStick.y, lt, rt);
        }
    }

    // Smooth the velocity toward the input direction instead of snapping.
    // Instant full-speed-on-keydown / dead-stop-on-release is what made the
    // fly cam feel stiff; a short frame-rate-independent ramp (~150ms up,
    // slightly quicker glide down) matches how other editors feel.
    f32 length = movement.Length();
    Math::Vector3 targetVel(0.0f, 0.0f, 0.0f);
    if (length > 0.001f) {
        targetVel = movement * (speed / length);
    }
    f32 rate = (length > 0.001f) ? m_MoveAccel : m_MoveDecel;
    f32 alpha = 1.0f - std::exp(-rate * deltaTime);
    m_Velocity.x += (targetVel.x - m_Velocity.x) * alpha;
    m_Velocity.y += (targetVel.y - m_Velocity.y) * alpha;
    m_Velocity.z += (targetVel.z - m_Velocity.z) * alpha;
    // Snap tiny residuals to zero so the camera never creeps forever
    if (length <= 0.001f && m_Velocity.Length() < 0.01f) {
        m_Velocity = Math::Vector3(0.0f, 0.0f, 0.0f);
    }
    if (m_Velocity.Length() > 0.0001f) {
        m_Camera->SetPosition(m_Camera->GetPosition() + m_Velocity * deltaTime);
    }

    // Scroll to adjust speed — only while look-dragging or over the viewport,
    // so scrolling the Inspector/Hierarchy doesn't silently change fly speed
    if (m_MouseCapturedByUs || m_PointerOverViewport) {
        Math::Vector2 scroll = Input::GetScrollDelta();
        if (scroll.y != 0.0f) {
            m_MoveSpeed *= (1.0f + scroll.y * 0.1f);
            m_MoveSpeed = Math::Clamp(m_MoveSpeed, 0.1f, 100.0f);
        }
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

    // Hold right mouse button to orbit (capture may only start over the viewport)
    bool rightMouseDown = Input::IsMouseButtonDown(MouseButton::Right) &&
                          (m_MouseCapturedByUs || m_PointerOverViewport);

    if (rightMouseDown) {
        if (!m_MouseCapturedByUs) {
            Input::SetMouseCaptured(true);
            m_MouseCapturedByUs = true;
            SyncFromCamera();
            m_LookWarmupFrames = 1;  // capture transition: same guard as fly mode
        } else if (m_LookWarmupFrames > 0) {
            --m_LookWarmupFrames;
        } else {
            Math::Vector2 mouseDelta = Input::GetMouseDelta();
            if (mouseDelta.Length() > 300.0f) {
                mouseDelta = Math::Vector2(0.0f, 0.0f);  // warp artifact, not a flick
            }
            m_Yaw += mouseDelta.x * m_LookSensitivity;
            m_Pitch -= mouseDelta.y * m_LookSensitivity;  // Inverted: drag up → look up
            m_Pitch = Math::Clamp(m_Pitch, -89.0f, 89.0f);
        }
    } else if (m_MouseCapturedByUs) {
        Input::SetMouseCaptured(false);
        m_MouseCapturedByUs = false;
    }

    // Middle mouse to pan (viewport only)
    bool middleMouseDown = Input::IsMouseButtonDown(MouseButton::Middle) &&
                           (m_MouseCapturedByUs || m_PointerOverViewport);
    if (middleMouseDown) {
        Math::Vector2 mouseDelta = Input::GetMouseDelta();
        Math::Vector3 right = m_Camera->GetRight();
        Math::Vector3 up = m_Camera->GetUp();
        f32 panSpeed = m_OrbitDistance * 0.002f;
        m_OrbitTarget = m_OrbitTarget - right * mouseDelta.x * panSpeed;
        m_OrbitTarget = m_OrbitTarget + up * mouseDelta.y * panSpeed;
    }

    // Scroll to zoom — viewport only, so panel scrolling doesn't zoom the orbit
    if (m_MouseCapturedByUs || m_PointerOverViewport) {
        Math::Vector2 scroll = Input::GetScrollDelta();
        if (scroll.y != 0.0f) {
            m_OrbitDistance *= (1.0f - scroll.y * 0.1f);
            m_OrbitDistance = Math::Clamp(m_OrbitDistance, m_MinOrbitDistance, m_MaxOrbitDistance);
        }
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
            m_Camera->SetPerspective(45.0f, m_Aspect, 0.1f, 1000.0f);
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
        f32 halfHeight = m_OrthoSize * 0.5f;
        f32 halfWidth = halfHeight * m_Aspect;
        m_Camera->SetOrthographic(-halfWidth, halfWidth, -halfHeight, halfHeight, 0.1f, 1000.0f);
    } else {
        m_Camera->SetPerspective(45.0f, m_Aspect, 0.1f, 1000.0f);
    }
}

void CameraController::SetViewportAspect(f32 aspect) {
    if (!m_Camera || aspect <= 0.0f) return;
    // Called every frame from the viewport — skip the projection rebuild when nothing
    // moved. Only the aspect changes here; FOV / near / far (and ortho size) are kept.
    f32 diff = aspect - m_Aspect;
    if ((diff < 0.0f ? -diff : diff) < 0.0001f) return;

    m_Aspect = aspect;
    if (m_IsOrthographic) {
        f32 halfHeight = m_OrthoSize * 0.5f;
        f32 halfWidth = halfHeight * m_Aspect;
        m_Camera->SetOrthographic(-halfWidth, halfWidth, -halfHeight, halfHeight,
                                  m_Camera->GetNearPlane(), m_Camera->GetFarPlane());
    } else {
        m_Camera->SetPerspective(m_Camera->GetFOV(), m_Aspect,
                                 m_Camera->GetNearPlane(), m_Camera->GetFarPlane());
    }
}

} // namespace Renderer
} // namespace Enjin
