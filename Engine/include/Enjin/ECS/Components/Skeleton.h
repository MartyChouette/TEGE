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
    std::string sourceAssetPath;  // Original glTF/FBX path for potential re-import
};

// Animator component - drives skeletal animation playback
struct ENJIN_API AnimatorComponent : public IComponent {
    Animation::SkeletalAnimator animator;
    Animation::AnimationStateMachine stateMachine;
    bool matricesDirty = true;

    // Editor debug: draw wireframe skeleton lines when selected
    bool showBones = false;

    // Blend tree: parameter-driven animation blending
    Animation::BlendTree blendTree;
    std::unordered_map<std::string, f32> blendParameters;  // Runtime parameter values

    void Initialize(std::shared_ptr<Animation::Skeleton> skel) {
        animator.SetSkeleton(skel);
        stateMachine.SetAnimator(&animator);
    }

    void SetBlendParameter(const std::string& name, f32 value) {
        blendParameters[name] = value;
    }

    f32 GetBlendParameter(const std::string& name) const {
        auto it = blendParameters.find(name);
        return (it != blendParameters.end()) ? it->second : 0.0f;
    }

    void Update(f32 deltaTime) {
        stateMachine.Update(deltaTime);

        // If blend tree is enabled and has a valid parameter, use it instead of normal playback
        if (blendTree.enabled && !blendTree.parameterName.empty() && blendTree.nodes.size() >= 2) {
            f32 paramValue = GetBlendParameter(blendTree.parameterName);
            animator.UpdateBlendTree(blendTree, paramValue, deltaTime);
        } else {
            animator.Update(deltaTime);
        }
        matricesDirty = true;
    }
};

} // namespace ECS
} // namespace Enjin
