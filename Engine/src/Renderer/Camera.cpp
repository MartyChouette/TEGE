#include "Enjin/Renderer/Camera.h"
#include "Enjin/Math/Math.h"

namespace Enjin {
namespace Renderer {

Camera::Camera() {
    SetPerspective(45.0f, 16.0f / 9.0f, 0.1f, 100.0f);
}

Camera::~Camera() {
}

void Camera::SetPosition(const Math::Vector3& position) {
    m_Position = position;
    m_ViewDirty = true;
}

void Camera::SetRotation(const Math::Quaternion& rotation) {
    m_Rotation = rotation;
    m_ViewDirty = true;
}

void Camera::SetLookAt(const Math::Vector3& eye, const Math::Vector3& center, const Math::Vector3& up) {
    m_Position = eye;
    Math::Vector3 forward = (center - eye).Normalized();
    Math::Vector3 right = forward.Cross(up).Normalized();
    Math::Vector3 actualUp = right.Cross(forward);

    // Build the view matrix directly from basis vectors (LookAt matrix)
    // This is the standard OpenGL-style LookAt: rotation^T * translation
    m_ViewMatrix = Math::Matrix4::Identity();
    m_ViewMatrix.m[0]  = right.x;
    m_ViewMatrix.m[4]  = right.y;
    m_ViewMatrix.m[8]  = right.z;
    m_ViewMatrix.m[1]  = actualUp.x;
    m_ViewMatrix.m[5]  = actualUp.y;
    m_ViewMatrix.m[9]  = actualUp.z;
    m_ViewMatrix.m[2]  = -forward.x;
    m_ViewMatrix.m[6]  = -forward.y;
    m_ViewMatrix.m[10] = -forward.z;
    // Translation component: dot products of basis with -eye
    m_ViewMatrix.m[12] = -(right.x * eye.x + right.y * eye.y + right.z * eye.z);
    m_ViewMatrix.m[13] = -(actualUp.x * eye.x + actualUp.y * eye.y + actualUp.z * eye.z);
    m_ViewMatrix.m[14] = (forward.x * eye.x + forward.y * eye.y + forward.z * eye.z);

    // Keep m_Rotation in sync so SetPosition doesn't rebuild from stale rotation.
    // m_Rotation must be the camera's WORLD rotation: basis vectors go in the
    // COLUMNS (column-major m[0..2] = col 0 = right, m[4..6] = col 1 = up,
    // m[8..10] = col 2 = -forward). The previous code spread them across ROWS,
    // storing the INVERSE rotation — after any look-at (F-focus, orbit,
    // presets), the first position-only change rebuilt the view from that
    // inverted rotation and the scene-view look direction flipped/skewed on
    // every WASD press (Marty, 2026-08-07).
    Math::Matrix4 rotMat = Math::Matrix4::Identity();
    rotMat.m[0] = right.x;      rotMat.m[1] = right.y;      rotMat.m[2]  = right.z;
    rotMat.m[4] = actualUp.x;   rotMat.m[5] = actualUp.y;   rotMat.m[6]  = actualUp.z;
    rotMat.m[8] = -forward.x;   rotMat.m[9] = -forward.y;   rotMat.m[10] = -forward.z;
    m_Rotation = Math::Quaternion::FromMatrix(rotMat);

    m_ViewDirty = false; // Already computed
}

void Camera::SetPerspective(f32 fov, f32 aspect, f32 nearPlane, f32 farPlane) {
    m_IsPerspective = true;
    m_Fov = fov;
    m_Aspect = aspect;
    m_NearPlane = nearPlane;
    m_FarPlane = farPlane;
    m_ProjectionDirty = true;
}

void Camera::SetOrthographic(f32 left, f32 right, f32 bottom, f32 top, f32 nearPlane, f32 farPlane) {
    m_IsPerspective = false;
    m_Left = left;
    m_Right = right;
    m_Bottom = bottom;
    m_Top = top;
    m_NearPlane = nearPlane;
    m_FarPlane = farPlane;
    m_ProjectionDirty = true;
}

Math::Matrix4 Camera::GetViewMatrix() const {
    if (m_ViewDirty) {
        Math::Matrix4 translation = Math::Matrix4::Translation(-m_Position);
        Math::Matrix4 rotation = m_Rotation.ToMatrix().Transposed(); // Inverse rotation
        m_ViewMatrix = rotation * translation;
        m_ViewDirty = false;
    }
    return m_ViewMatrix;
}

Math::Matrix4 Camera::GetProjectionMatrix() const {
    if (m_ProjectionDirty) {
        if (m_IsPerspective) {
            m_ProjectionMatrix = Math::Matrix4::Perspective(
                Math::Radians(m_Fov), m_Aspect, m_NearPlane, m_FarPlane);
        } else {
            m_ProjectionMatrix = Math::Matrix4::Orthographic(
                m_Left, m_Right, m_Bottom, m_Top, m_NearPlane, m_FarPlane);
        }
        m_ProjectionDirty = false;
    }
    return m_ProjectionMatrix;
}

Math::Matrix4 Camera::GetViewProjectionMatrix() const {
    return GetProjectionMatrix() * GetViewMatrix();
}

Math::Vector3 Camera::GetForward() const {
    // m_Rotation is the camera's WORLD rotation; ToMatrix stores the basis in
    // COLUMNS (column-major: col0 = m[0..2] right, col1 = m[4..6] up,
    // col2 = m[8..10] = -forward — matches GetViewMatrix's ToMatrix().Transposed()).
    // These getters used to read ROWS (the transpose): paired with the old
    // inverse-storing SetLookAt the errors cancelled, but they made RMB-grab
    // snap to a mirrored direction once SetLookAt stored the true rotation
    // (Marty, 2026-08-07).
    Math::Matrix4 rot = m_Rotation.ToMatrix();
    return Math::Vector3(-rot.m[8], -rot.m[9], -rot.m[10]).Normalized();
}

Math::Vector3 Camera::GetRight() const {
    Math::Matrix4 rot = m_Rotation.ToMatrix();
    return Math::Vector3(rot.m[0], rot.m[1], rot.m[2]).Normalized();
}

Math::Vector3 Camera::GetUp() const {
    Math::Matrix4 rot = m_Rotation.ToMatrix();
    return Math::Vector3(rot.m[4], rot.m[5], rot.m[6]).Normalized();
}

void Camera::UpdateViewMatrix() {
    m_ViewDirty = true;
}

} // namespace Renderer
} // namespace Enjin
