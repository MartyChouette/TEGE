#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Matrix.h"
#include "Enjin/Math/Quaternion.h"

namespace Enjin {
namespace Renderer {

// 3D Camera for view/projection matrices
class ENJIN_API Camera {
public:
    Camera();
    ~Camera();

    // Set position and rotation
    void SetPosition(const Math::Vector3& position);
    void SetRotation(const Math::Quaternion& rotation);
    void SetLookAt(const Math::Vector3& eye, const Math::Vector3& center, const Math::Vector3& up);

    // Projection settings
    void SetPerspective(f32 fov, f32 aspect, f32 nearPlane, f32 farPlane);
    void SetOrthographic(f32 left, f32 right, f32 bottom, f32 top, f32 nearPlane, f32 farPlane);

    // Get matrices
    Math::Matrix4 GetViewMatrix() const;
    Math::Matrix4 GetProjectionMatrix() const;
    Math::Matrix4 GetViewProjectionMatrix() const;

    // Getters
    Math::Vector3 GetPosition() const { return m_Position; }
    Math::Quaternion GetRotation() const { return m_Rotation; }
    Math::Vector3 GetForward() const;
    Math::Vector3 GetRight() const;
    Math::Vector3 GetUp() const;

    // Projection getters
    f32 GetFOV() const { return m_Fov; }
    f32 GetAspect() const { return m_Aspect; }
    f32 GetNearPlane() const { return m_NearPlane; }
    f32 GetFarPlane() const { return m_FarPlane; }
    bool IsPerspective() const { return m_IsPerspective; }

private:
    void UpdateViewMatrix();

    Math::Vector3 m_Position = Math::Vector3(0.0f, 0.0f, 0.0f);
    Math::Quaternion m_Rotation = Math::Quaternion::Identity();
    
    // Projection
    bool m_IsPerspective = true;
    f32 m_Fov = 45.0f;
    f32 m_Aspect = 16.0f / 9.0f;
    f32 m_NearPlane = 0.1f;
    f32 m_FarPlane = 100.0f;
    
    // Orthographic
    f32 m_Left = -1.0f;
    f32 m_Right = 1.0f;
    f32 m_Bottom = -1.0f;
    f32 m_Top = 1.0f;

    mutable Math::Matrix4 m_ViewMatrix;
    mutable Math::Matrix4 m_ProjectionMatrix;
    mutable bool m_ViewDirty = true;
    mutable bool m_ProjectionDirty = true;
};

} // namespace Renderer
} // namespace Enjin
