#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/Renderer/Camera.h"
#include "Enjin/Math/Vector.h"

namespace Enjin {
namespace Gameplay {

// CameraDirector — the "brain" of the virtual-camera system.
//
// SINGLE-OWNER INVARIANT: while it is active, the Director is the ONLY thing
// that writes the game camera's transform. It picks the highest-priority live
// VirtualCameraComponent, computes that vcam's desired pose, and blends the
// real camera toward it. Everything else (controllers, scripts) drives *vcams*,
// not the camera — that is what makes direct manipulation safe to expose.
//
// Three tiers of access:
//   Tier 1 (editor)   — author vcams, set priority/targets. Can't fight it.
//   Tier 2 (directed) — scripts raise a vcam's priority / request a blend. The
//                       Director still owns the transform and honors the request.
//   Tier 3 (manual)   — TakeManualControl() makes the Director YIELD: it stops
//                       writing the transform until ReleaseManualControl(), which
//                       blends back to the live vcam. Control is a token you hold.
//
// When a scene has no vcams, the Director does nothing and the existing camera
// path (controller/cinematic) is untouched — adopting it is opt-in per scene.
class ENJIN_API CameraDirector {
public:
    struct Pose {
        Math::Vector3 position = Math::Vector3(0, 0, 0);
        Math::Vector3 lookPoint = Math::Vector3(0, 0, -1);
        f32 fov = 55.0f;
    };

    // Drive the camera for one frame. Safe to call every frame even with no vcams.
    void Update(ECS::World* world, Renderer::Camera* gameCamera, f32 deltaTime);

    void SetEnabled(bool enabled) { m_Enabled = enabled; }
    bool IsEnabled() const { return m_Enabled; }

    // Reset blend state (call on play start / scene load).
    void Reset();

    // --- Tier 3: manual-control token ---
    void TakeManualControl(ECS::Entity owner = 1);
    void ReleaseManualControl();
    bool IsManual() const { return m_Manual; }

    // Introspection for the editor Director panel.
    ECS::Entity GetLiveVCam() const { return m_LiveVCam; }
    bool IsBlending() const { return m_Blending; }
    f32 GetBlendProgress() const { return m_Blending ? m_BlendT : 1.0f; }

private:
    bool ComputeVCamPose(ECS::World* world, ECS::Entity vcam, Pose& out) const;

    bool m_Enabled = false;

    ECS::Entity m_LiveVCam = 0;
    Pose m_Current;
    bool m_HasPose = false;

    // explicit blend on a vcam switch
    bool m_Blending = false;
    f32 m_BlendT = 0.0f;
    f32 m_BlendDur = 0.5f;
    Pose m_BlendFrom;

    // manual override (Tier 3)
    bool m_Manual = false;
    ECS::Entity m_ManualOwner = 0;
};

} // namespace Gameplay
} // namespace Enjin
