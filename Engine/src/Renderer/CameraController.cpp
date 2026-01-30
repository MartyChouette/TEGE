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

    // Extract yaw and pitch from the camera's forward vector
    Math::Vector3 forward = m_Camera->GetForward();

    // Pitch is the vertical angle
    // forward.y = sin(pitch) when cos(pitch) ≈ 1
    m_Pitch = Math::Degrees(Math::Asin(Math::Clamp(forward.y, -1.0f, 1.0f)));

    // Yaw is the horizontal angle
    // At yaw=0: forward.x = 0, forward.z = -1
    // forward.x = sin(yaw) * cos(pitch)
    // forward.z = -cos(yaw) * cos(pitch)
    // So yaw = atan2(forward.x, -forward.z)
    m_Yaw = Math::Degrees(Math::Atan2(forward.x, -forward.z));

    // Clamp pitch
    m_Pitch = Math::Clamp(m_Pitch, -89.0f, 89.0f);
}

void CameraController::Update(f32 deltaTime) {
    if (!m_Camera || !m_Enabled) {
        // Release mouse when controller is disabled
        if (m_MouseCapturedByUs) {
            Input::SetMouseCaptured(false);
            m_MouseCapturedByUs = false;
        }
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
    // Capture mouse and always apply mouse look
    if (!m_MouseCapturedByUs) {
        Input::SetMouseCaptured(true);
        m_MouseCapturedByUs = true;
        SyncFromCamera();
    }

    // Mouse look - always active
    Math::Vector2 mouseDelta = Input::GetMouseDelta();
    m_Yaw -= mouseDelta.x * m_LookSensitivity;
    m_Pitch += mouseDelta.y * m_LookSensitivity;
    m_Pitch = Math::Clamp(m_Pitch, -89.0f, 89.0f);
    ApplyRotation();

    // Movement - FPS-style: WASD on horizontal plane, Space/Ctrl for vertical
    f32 speed = m_MoveSpeed;
    if (Input::IsKeyDown(KeyCode::LeftShift) || Input::IsKeyDown(KeyCode::RightShift)) {
        speed *= m_SprintMultiplier;
    }

    Math::Vector3 movement(0.0f, 0.0f, 0.0f);

    // Project camera vectors onto the horizontal (XZ) plane for WASD movement
    // Forward: flatten the camera's look direction to the ground plane
    Math::Vector3 fullForward = m_Camera->GetForward();
    Math::Vector3 forward(fullForward.x, 0.0f, fullForward.z);
    f32 fwdLen = forward.Length();
    if (fwdLen > 0.001f) {
        forward = forward * (1.0f / fwdLen);
    } else {
        // Looking straight up/down: use up vector projected to XZ as forward
        Math::Vector3 camUp = m_Camera->GetUp();
        forward = Math::Vector3(camUp.x, 0.0f, camUp.z).Normalized();
    }

    // Right: flatten the camera's right vector to the ground plane
    Math::Vector3 fullRight = m_Camera->GetRight();
    Math::Vector3 right(fullRight.x, 0.0f, fullRight.z);
    right.Normalize();

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
    if (Input::IsKeyDown(KeyCode::Space)) {
        movement = movement + worldUp;
    }
    if (Input::IsKeyDown(KeyCode::Q) || Input::IsKeyDown(KeyCode::LeftControl)) {
        movement = movement - worldUp;
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
}

void CameraController::UpdateOrbitMode(f32 deltaTime) {
    (void)deltaTime;

    // Capture mouse and always apply orbit rotation
    if (!m_MouseCapturedByUs) {
        Input::SetMouseCaptured(true);
        m_MouseCapturedByUs = true;
        SyncFromCamera();
    }

    Math::Vector2 mouseDelta = Input::GetMouseDelta();
    m_Yaw -= mouseDelta.x * m_LookSensitivity;
    m_Pitch += mouseDelta.y * m_LookSensitivity;
    m_Pitch = Math::Clamp(m_Pitch, -89.0f, 89.0f);

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

    // Calculate camera position from orbit parameters
    f32 yawRad = Math::Radians(m_Yaw);
    f32 pitchRad = Math::Radians(m_Pitch);

    Math::Vector3 offset;
    offset.x = Math::Cos(pitchRad) * Math::Cos(yawRad) * m_OrbitDistance;
    offset.y = Math::Sin(pitchRad) * m_OrbitDistance;
    offset.z = Math::Cos(pitchRad) * Math::Sin(yawRad) * m_OrbitDistance;

    Math::Vector3 cameraPos = m_OrbitTarget + offset;
    m_Camera->SetPosition(cameraPos);

    // Look at target
    m_Camera->SetLookAt(cameraPos, m_OrbitTarget, Math::Vector3(0.0f, 1.0f, 0.0f));
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
    // Convert euler angles to quaternion
    f32 yawRad = Math::Radians(m_Yaw);
    f32 pitchRad = Math::Radians(m_Pitch);

    // Create rotation quaternion from yaw and pitch
    Math::Quaternion yawQuat(Math::Vector3(0.0f, 1.0f, 0.0f), yawRad);
    Math::Quaternion pitchQuat(Math::Vector3(1.0f, 0.0f, 0.0f), pitchRad);

    // Yaw first, then pitch
    Math::Quaternion rotation = yawQuat * pitchQuat;
    m_Camera->SetRotation(rotation);
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

        case ViewPreset::Top:
            m_Yaw = -90.0f;
            m_Pitch = -89.0f;  // Looking straight down
            m_Camera->SetPosition(target + Math::Vector3(0.0f, distance, 0.0f));
            break;

        case ViewPreset::Bottom:
            m_Yaw = -90.0f;
            m_Pitch = 89.0f;   // Looking straight up
            m_Camera->SetPosition(target + Math::Vector3(0.0f, -distance, 0.0f));
            break;

        case ViewPreset::Front:
            m_Yaw = -90.0f;
            m_Pitch = 0.0f;
            m_Camera->SetPosition(target + Math::Vector3(0.0f, 0.0f, distance));
            break;

        case ViewPreset::Back:
            m_Yaw = 90.0f;
            m_Pitch = 0.0f;
            m_Camera->SetPosition(target + Math::Vector3(0.0f, 0.0f, -distance));
            break;

        case ViewPreset::Right:
            m_Yaw = 0.0f;
            m_Pitch = 0.0f;
            m_Camera->SetPosition(target + Math::Vector3(distance, 0.0f, 0.0f));
            break;

        case ViewPreset::Left:
            m_Yaw = 180.0f;
            m_Pitch = 0.0f;
            m_Camera->SetPosition(target + Math::Vector3(-distance, 0.0f, 0.0f));
            break;
    }

    // Apply rotation for preset views
    ApplyRotation();

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
