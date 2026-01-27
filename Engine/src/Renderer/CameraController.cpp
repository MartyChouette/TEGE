#include "Enjin/Renderer/CameraController.h"
#include "Enjin/Platform/Input.h"
#include "Enjin/Math/Math.h"
#include "Enjin/Math/Quaternion.h"

namespace Enjin {
namespace Renderer {

CameraController::CameraController(Camera* camera)
    : m_Camera(camera) {
}

void CameraController::Update(f32 deltaTime) {
    if (!m_Camera || !m_Enabled) {
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
    // Check if right mouse is held for look control
    bool rightMouseDown = Input::IsMouseButtonDown(MouseButton::Right);

    if (rightMouseDown) {
        if (!m_RightMouseHeld) {
            // Just pressed - capture mouse
            Input::SetMouseCaptured(true);
            m_RightMouseHeld = true;
        }

        // Look around with mouse delta
        Math::Vector2 mouseDelta = Input::GetMouseDelta();
        m_Yaw += mouseDelta.x * m_LookSensitivity;
        m_Pitch -= mouseDelta.y * m_LookSensitivity;

        // Clamp pitch to prevent flipping
        m_Pitch = Math::Clamp(m_Pitch, -89.0f, 89.0f);

        ApplyRotation();
    } else if (m_RightMouseHeld) {
        // Just released - release mouse
        Input::SetMouseCaptured(false);
        m_RightMouseHeld = false;
    }

    // Movement with WASD/QE
    f32 speed = m_MoveSpeed;
    if (Input::IsKeyDown(KeyCode::LeftShift) || Input::IsKeyDown(KeyCode::RightShift)) {
        speed *= m_SprintMultiplier;
    }

    Math::Vector3 movement(0.0f, 0.0f, 0.0f);
    Math::Vector3 forward = m_Camera->GetForward();
    Math::Vector3 right = m_Camera->GetRight();
    Math::Vector3 up = Math::Vector3(0.0f, 1.0f, 0.0f); // World up

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
    if (Input::IsKeyDown(KeyCode::E) || Input::IsKeyDown(KeyCode::Space)) {
        movement = movement + up;
    }
    if (Input::IsKeyDown(KeyCode::Q) || Input::IsKeyDown(KeyCode::LeftControl)) {
        movement = movement - up;
    }

    // Normalize and apply movement
    f32 length = movement.Length();
    if (length > 0.001f) {
        movement = movement * (1.0f / length);
        Math::Vector3 newPos = m_Camera->GetPosition() + movement * speed * deltaTime;
        m_Camera->SetPosition(newPos);
    }

    // Scroll to adjust speed
    Math::Vector2 scroll = Input::GetScrollDelta();
    if (scroll.y != 0.0f) {
        m_MoveSpeed *= (1.0f + scroll.y * 0.1f);
        m_MoveSpeed = Math::Clamp(m_MoveSpeed, 0.1f, 100.0f);
    }
}

void CameraController::UpdateOrbitMode(f32 deltaTime) {
    (void)deltaTime;

    bool rightMouseDown = Input::IsMouseButtonDown(MouseButton::Right);

    if (rightMouseDown) {
        if (!m_RightMouseHeld) {
            Input::SetMouseCaptured(true);
            m_RightMouseHeld = true;
        }

        // Orbit with mouse delta
        Math::Vector2 mouseDelta = Input::GetMouseDelta();
        m_Yaw += mouseDelta.x * m_LookSensitivity;
        m_Pitch -= mouseDelta.y * m_LookSensitivity;
        m_Pitch = Math::Clamp(m_Pitch, -89.0f, 89.0f);
    } else if (m_RightMouseHeld) {
        Input::SetMouseCaptured(false);
        m_RightMouseHeld = false;
    }

    // Middle mouse or Shift+RMB to pan
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

    // Constrain to ground plane (y = fixed height)
    Math::Vector3 pos = m_Camera->GetPosition();
    pos.y = 1.7f; // Eye height
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

} // namespace Renderer
} // namespace Enjin
