#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Quaternion.h"
#include "Enjin/ECS/Entity.h"
#include <vector>
#include <string>

namespace Enjin::Gameplay {

// Channel flags — select which data domains to record/restore
enum class RewindChannelFlags : u32 {
    None          = 0,
    Transform     = 1 << 0,   // position, rotation, scale, visible
    Velocity      = 1 << 1,   // controller velocity
    Health        = 1 << 2,   // currentHealth, isDead, shield
    Animation     = 1 << 3,   // animator time, current clip index
    Physics       = 1 << 4,   // physics body linear/angular velocity
    Material      = 1 << 5,   // material color/opacity overrides
    All           = 0x3F,
    Default       = 0x07,     // Transform | Velocity | Health
};

inline RewindChannelFlags operator|(RewindChannelFlags a, RewindChannelFlags b) {
    return static_cast<RewindChannelFlags>(static_cast<u32>(a) | static_cast<u32>(b));
}
inline RewindChannelFlags operator&(RewindChannelFlags a, RewindChannelFlags b) {
    return static_cast<RewindChannelFlags>(static_cast<u32>(a) & static_cast<u32>(b));
}
inline bool HasChannel(u32 mask, RewindChannelFlags ch) {
    return (mask & static_cast<u32>(ch)) != 0;
}

// Per-entity snapshot — stores state for all enabled channels
struct EntitySnapshot {
    ECS::Entity entity = ECS::INVALID_ENTITY;
    u32 channelMask = 0;

    // Transform channel
    Math::Vector3 position;
    Math::Quaternion rotation;
    Math::Vector3 scale = Math::Vector3(1.0f);
    bool visible = true;

    // Velocity channel
    Math::Vector3 velocity;

    // Health channel
    f32 health = 0.0f;
    f32 shield = 0.0f;
    bool isDead = false;

    // Animation channel
    f32 animNormalizedTime = 0.0f;
    std::string animClipName;

    // Physics channel
    Math::Vector3 linearVelocity;
    Math::Vector3 angularVelocity;

    // Material channel
    f32 opacity = 1.0f;
    Math::Vector3 baseColor = Math::Vector3(1.0f);
};

// A frame in the delta-compressed scene history
struct DeltaFrame {
    f32 timestamp = 0.0f;
    bool isKeyframe = false;
    std::vector<EntitySnapshot> snapshots; // all entities if keyframe, changed-only if delta
};

} // namespace Enjin::Gameplay
