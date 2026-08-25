#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/ECS/Entity.h"

namespace Enjin {
namespace ECS {

// VirtualCameraComponent — a "vcam" in the Cinemachine sense.
//
// Dead-simple contract: point it at a target to FOLLOW, pick a SHOT PRESET,
// set a PRIORITY. The CameraDirector activates the highest-priority live vcam
// and blends the real camera to it. You never touch the real camera — the
// Director owns it (single-owner invariant). Two vcams + a priority swap is a
// whole camera system (e.g. isometric vs over-the-shoulder).
//
// The preset is the tutorialless front door: pick "Over-the-Shoulder" and the
// offset/fov/damping fill in with a good shot. Everything stays editable after,
// so the preset is a starting point, not a lock.

// Named director shot presets. Each seeds the vcam's framing fields.
enum class VCamShot : u8 {
    Custom = 0,     // hand-tuned; no preset applied
    Isometric,      // high angled overview (the Hades read)
    OverShoulder,   // low, close, behind the character's facing (the Souls read)
    Follow,         // standard third-person, behind and above
    TopDown,        // straight down
    CloseUp,        // tight on the subject, dramatic
    Wide,           // pulled back, establishing
    SideScroll,     // camera to the side, looking across (2.5D)
    BirdsEye,       // very high, map-like
    Count
};

inline const char* VCamShotName(VCamShot s) {
    switch (s) {
        case VCamShot::Custom:       return "Custom";
        case VCamShot::Isometric:    return "Isometric";
        case VCamShot::OverShoulder: return "Over-the-Shoulder";
        case VCamShot::Follow:       return "Follow";
        case VCamShot::TopDown:      return "Top-Down";
        case VCamShot::CloseUp:      return "Close-Up";
        case VCamShot::Wide:         return "Wide / Establishing";
        case VCamShot::SideScroll:   return "Side-Scroller";
        case VCamShot::BirdsEye:     return "Bird's Eye";
        default:                     return "Unknown";
    }
}

struct VirtualCameraComponent {
    bool enabled = true;
    i32 priority = 10;              // highest enabled vcam wins

    // Which preset last seeded the framing (display + re-apply). Custom = tweaked.
    VCamShot shot = VCamShot::Custom;

    // Targets
    Entity follow = 0;             // anchor entity (0 = use this entity's own transform)
    Entity lookAt = 0;             // aim entity (0 = look at the follow anchor)

    // Body: camera position = anchor + offset. When offsetInFollowSpace is true,
    // the offset is rotated by the follow target's yaw, so the shot stays behind
    // the character as it turns (over-shoulder, follow). When false, the offset
    // is in world axes (isometric, top-down, side-scroller — fixed to the world).
    Math::Vector3 offset = Math::Vector3(0.0f, 6.0f, 10.0f);
    bool offsetInFollowSpace = false;

    // Aim: look point = (lookAt or anchor) + lookOffset (world axes; usually vertical)
    Math::Vector3 lookOffset = Math::Vector3(0.0f, 1.0f, 0.0f);

    // Lens
    f32 fov = 55.0f;

    // Feel — seconds to catch up to a moving target (0 = rigid snap). This is the
    // damped-follow term; the Director uses frame-rate-independent smoothing.
    f32 damping = 0.3f;

    // Blend duration used when THIS vcam becomes the live one.
    f32 blendTime = 0.5f;

    // --- runtime (serialized-harmless; the Director maintains these) ---
    bool isLive = false;
};

// Seed a vcam's framing from a preset. Modular by design: it only writes the
// framing fields (offset / lookOffset / fov / damping / blendTime / space), so
// priority, targets and enabled state are preserved. Custom leaves them alone.
inline void ApplyVCamPreset(VirtualCameraComponent& vc, VCamShot shot) {
    vc.shot = shot;
    switch (shot) {
        case VCamShot::Isometric:
            vc.offset = Math::Vector3(0.0f, 15.0f, 13.0f);
            vc.lookOffset = Math::Vector3(0.0f, 1.0f, 0.0f);
            vc.fov = 55.0f; vc.damping = 0.35f; vc.blendTime = 0.5f;
            vc.offsetInFollowSpace = false;
            break;
        case VCamShot::OverShoulder:
            // behind (-Z in follow space) + right + up
            vc.offset = Math::Vector3(0.9f, 1.7f, -3.2f);
            vc.lookOffset = Math::Vector3(0.0f, 1.4f, 0.0f);
            vc.fov = 55.0f; vc.damping = 0.14f; vc.blendTime = 0.4f;
            vc.offsetInFollowSpace = true;
            break;
        case VCamShot::Follow:
            vc.offset = Math::Vector3(0.0f, 3.5f, -6.0f);
            vc.lookOffset = Math::Vector3(0.0f, 1.2f, 0.0f);
            vc.fov = 60.0f; vc.damping = 0.2f; vc.blendTime = 0.45f;
            vc.offsetInFollowSpace = true;
            break;
        case VCamShot::TopDown:
            vc.offset = Math::Vector3(0.0f, 18.0f, 0.01f);
            vc.lookOffset = Math::Vector3(0.0f, 0.0f, 0.0f);
            vc.fov = 50.0f; vc.damping = 0.3f; vc.blendTime = 0.5f;
            vc.offsetInFollowSpace = false;
            break;
        case VCamShot::CloseUp:
            vc.offset = Math::Vector3(0.0f, 1.6f, -2.0f);
            vc.lookOffset = Math::Vector3(0.0f, 1.5f, 0.0f);
            vc.fov = 40.0f; vc.damping = 0.12f; vc.blendTime = 0.35f;
            vc.offsetInFollowSpace = true;
            break;
        case VCamShot::Wide:
            vc.offset = Math::Vector3(0.0f, 6.0f, 16.0f);
            vc.lookOffset = Math::Vector3(0.0f, 1.0f, 0.0f);
            vc.fov = 70.0f; vc.damping = 0.4f; vc.blendTime = 0.6f;
            vc.offsetInFollowSpace = false;
            break;
        case VCamShot::SideScroll:
            vc.offset = Math::Vector3(14.0f, 2.0f, 0.0f);
            vc.lookOffset = Math::Vector3(0.0f, 1.0f, 0.0f);
            vc.fov = 55.0f; vc.damping = 0.25f; vc.blendTime = 0.5f;
            vc.offsetInFollowSpace = false;
            break;
        case VCamShot::BirdsEye:
            vc.offset = Math::Vector3(0.0f, 30.0f, 0.01f);
            vc.lookOffset = Math::Vector3(0.0f, 0.0f, 0.0f);
            vc.fov = 55.0f; vc.damping = 0.4f; vc.blendTime = 0.6f;
            vc.offsetInFollowSpace = false;
            break;
        case VCamShot::Custom:
        default:
            // leave fields as they are
            break;
    }
}

} // namespace ECS
} // namespace Enjin
