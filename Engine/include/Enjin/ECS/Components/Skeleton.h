#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/Component.h"
#include "Enjin/Animation/Animation.h"
#include <memory>

namespace Enjin {
namespace ECS {

// Skeleton component - holds shared skeleton data for skinned meshes
struct ENJIN_API SkeletonComponent : public IComponent {
    std::shared_ptr<Animation::Skeleton> skeleton;
};

// Animator component - drives skeletal animation playback
struct ENJIN_API AnimatorComponent : public IComponent {
    Animation::SkeletalAnimator animator;
    Animation::AnimationStateMachine stateMachine;
    bool matricesDirty = true;

    void Initialize(std::shared_ptr<Animation::Skeleton> skel) {
        animator.SetSkeleton(skel);
        stateMachine.SetAnimator(&animator);
    }

    void Update(f32 deltaTime) {
        stateMachine.Update(deltaTime);
        animator.Update(deltaTime);
        matricesDirty = true;
    }
};

} // namespace ECS
} // namespace Enjin
