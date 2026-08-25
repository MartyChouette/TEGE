#include "Enjin/Gameplay/CameraDirector.h"
#include "Enjin/ECS/Components/VirtualCamera.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Camera.h"
#include "Enjin/Logging/Log.h"
#include <cmath>
#include <climits>

namespace Enjin {
namespace Gameplay {

// Frame-rate-independent exponential smoothing toward a target. `tau` is the
// approximate seconds-to-catch-up (the vcam's damping). tau<=0 snaps.
static Math::Vector3 SmoothTo(const Math::Vector3& cur, const Math::Vector3& tgt,
                              f32 tau, f32 dt) {
    if (tau <= 1e-4f || dt <= 0.0f) return tgt;
    f32 a = 1.0f - std::exp(-dt / tau);
    return cur + (tgt - cur) * a;
}

static f32 SmoothTo(f32 cur, f32 tgt, f32 tau, f32 dt) {
    if (tau <= 1e-4f || dt <= 0.0f) return tgt;
    f32 a = 1.0f - std::exp(-dt / tau);
    return cur + (tgt - cur) * a;
}

static f32 SmoothStep(f32 t) {
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    return t * t * (3.0f - 2.0f * t);
}

void CameraDirector::Reset() {
    m_LiveVCam = 0;
    m_HasPose = false;
    m_Blending = false;
    m_BlendT = 0.0f;
    m_Manual = false;
    m_ManualOwner = 0;
}

void CameraDirector::TakeManualControl(ECS::Entity owner) {
    m_Manual = true;
    m_ManualOwner = owner;
    // Drop any in-flight blend so the release starts clean from wherever the
    // manual controller left the camera.
    m_Blending = false;
    ENJIN_LOG_INFO(Editor, "CameraDirector: manual control taken (owner %llu)",
                   static_cast<unsigned long long>(owner));
}

void CameraDirector::ReleaseManualControl() {
    if (!m_Manual) return;
    m_Manual = false;
    m_ManualOwner = 0;
    // Force a blend back to the live vcam from the current (manually-left) pose.
    m_HasPose = true;
    m_Blending = true;
    m_BlendT = 0.0f;
    m_BlendFrom = m_Current;
    ENJIN_LOG_INFO(Editor, "CameraDirector: manual control released, blending back");
}

bool CameraDirector::ComputeVCamPose(ECS::World* world, ECS::Entity vcam, Pose& out) const {
    auto* vc = world->GetComponent<ECS::VirtualCameraComponent>(vcam);
    if (!vc) return false;

    // Anchor: the follow target's position, or the vcam's own transform.
    Math::Vector3 anchor(0, 0, 0);
    ECS::Entity anchorEnt = (vc->follow != 0 && world->IsValid(vc->follow)) ? vc->follow : vcam;
    if (auto* at = world->GetComponent<ECS::TransformComponent>(anchorEnt)) {
        anchor = at->position;
    }

    // Aim point: the lookAt target, or the anchor, plus lookOffset.
    Math::Vector3 aim = anchor;
    if (vc->lookAt != 0 && world->IsValid(vc->lookAt)) {
        if (auto* lt = world->GetComponent<ECS::TransformComponent>(vc->lookAt)) {
            aim = lt->position;
        }
    }

    out.position = anchor + vc->offset;
    out.lookPoint = aim + vc->lookOffset;
    out.fov = vc->fov;
    return true;
}

void CameraDirector::Update(ECS::World* world, Renderer::Camera* gameCamera, f32 deltaTime) {
    if (!m_Enabled || !world || !gameCamera) return;

    // Tier 3: yielded. The manual controller owns the transform; do nothing.
    if (m_Manual) return;

    // Pick the highest-priority enabled vcam.
    ECS::Entity best = 0;
    i32 bestPriority = INT_MIN;
    f32 liveDamping = 0.3f;
    for (ECS::Entity e : world->GetEntitiesWithComponent<ECS::VirtualCameraComponent>()) {
        auto* vc = world->GetComponent<ECS::VirtualCameraComponent>(e);
        if (!vc || !vc->enabled) continue;
        if (vc->priority > bestPriority) {
            bestPriority = vc->priority;
            best = e;
            liveDamping = vc->damping;
        }
    }

    // No vcams — Director is dormant, leave the camera to the controller/cinematic.
    if (best == 0) {
        // Clear stale isLive flags so re-adopting later starts fresh.
        if (m_LiveVCam != 0) { m_LiveVCam = 0; m_HasPose = false; }
        return;
    }

    // Desired pose from the live vcam.
    Pose target;
    if (!ComputeVCamPose(world, best, target)) return;

    // First adoption — snap, no blend.
    if (!m_HasPose) {
        m_Current = target;
        m_LiveVCam = best;
        m_HasPose = true;
        m_Blending = false;
    }

    // A switch to a different vcam starts an explicit blend from the current pose.
    if (best != m_LiveVCam) {
        auto* vc = world->GetComponent<ECS::VirtualCameraComponent>(best);
        m_Blending = true;
        m_BlendT = 0.0f;
        m_BlendDur = (vc && vc->blendTime > 0.0f) ? vc->blendTime : 0.5f;
        m_BlendFrom = m_Current;
        m_LiveVCam = best;
    }

    if (m_Blending) {
        m_BlendT += (m_BlendDur > 0.0f) ? (deltaTime / m_BlendDur) : 1.0f;
        f32 s = SmoothStep(m_BlendT);
        m_Current.position = m_BlendFrom.position + (target.position - m_BlendFrom.position) * s;
        m_Current.lookPoint = m_BlendFrom.lookPoint + (target.lookPoint - m_BlendFrom.lookPoint) * s;
        m_Current.fov = m_BlendFrom.fov + (target.fov - m_BlendFrom.fov) * s;
        if (m_BlendT >= 1.0f) m_Blending = false;
    } else {
        // Damped follow of the live vcam.
        m_Current.position = SmoothTo(m_Current.position, target.position, liveDamping, deltaTime);
        m_Current.lookPoint = SmoothTo(m_Current.lookPoint, target.lookPoint, liveDamping, deltaTime);
        m_Current.fov = SmoothTo(m_Current.fov, target.fov, liveDamping, deltaTime);
    }

    // --- write the real camera (the single owner) ---
    gameCamera->SetPosition(m_Current.position);
    Math::Vector3 dir = m_Current.lookPoint - m_Current.position;
    if (dir.Length() > 1e-5f) {
        gameCamera->SetLookAt(m_Current.position, m_Current.lookPoint, Math::Vector3(0, 1, 0));
    }
    gameCamera->SetPerspective(m_Current.fov, 16.0f / 9.0f, 0.1f, 1000.0f);

    // Mirror onto the active camera ENTITY transform — the GAME VIEW renders
    // from the CameraComponent entity's transform, not from the Renderer::Camera
    // above, so this write is what actually shows on screen (same path the
    // ThirdPersonController uses). Camera looks down its local -Z, and
    // LookRotation puts its `forward` arg on local +Z, so we pass -viewDir.
    ECS::Entity camEnt = ECS::CameraManager::GetActiveCamera(world);
    if (camEnt != ECS::INVALID_ENTITY) {
        if (auto* ct = world->GetComponent<ECS::TransformComponent>(camEnt)) {
            ct->position = m_Current.position;
            if (dir.Length() > 1e-5f) {
                Math::Vector3 fwd = dir.Normalized();
                ct->rotation = Math::Quaternion::LookRotation(fwd * -1.0f, Math::Vector3(0, 1, 0));
            }
        }
        if (auto* cc = world->GetComponent<ECS::CameraComponent>(camEnt)) {
            cc->fieldOfView = m_Current.fov;
        }
    }

    // Publish isLive flags for the editor panel.
    for (ECS::Entity e : world->GetEntitiesWithComponent<ECS::VirtualCameraComponent>()) {
        if (auto* vc = world->GetComponent<ECS::VirtualCameraComponent>(e)) {
            vc->isLive = (e == m_LiveVCam);
        }
    }
}

} // namespace Gameplay
} // namespace Enjin
