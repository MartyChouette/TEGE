#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/Component.h"
#include "Enjin/Animation/Animation.h"
#include "Enjin/Math/Vector.h"
#include <memory>

namespace Enjin {
namespace ECS {

// Skeleton component - holds shared skeleton data for skinned meshes
struct ENJIN_API SkeletonComponent : public IComponent {
    std::shared_ptr<Animation::Skeleton> skeleton;
    std::string sourceAssetPath;  // Original glTF/FBX path for potential re-import
};

// Onion skin settings for 3D skeletal animation preview in the editor
struct SkeletalOnionSkinSettings {
    bool enabled = false;
    i32 framesBefore = 3;
    i32 framesAfter = 3;
    f32 opacity = 0.25f;
    f32 opacityFalloff = 0.6f;       // Multiplier per step (closer = more opaque)
    Math::Vector3 beforeTint{0.3f, 0.5f, 1.0f};   // Blue-ish
    Math::Vector3 afterTint{1.0f, 0.4f, 0.3f};     // Red-ish
};

// Animator component - drives skeletal animation playback
struct ENJIN_API AnimatorComponent : public IComponent {
    Animation::SkeletalAnimator animator;
    Animation::AnimationStateMachine stateMachine;
    bool matricesDirty = true;

    // Editor debug: draw wireframe skeleton lines when selected
    bool showBones = false;

    // Editor: index of the selected bone in the viewport (-1 = none)
    i32 selectedBoneIndex = -1;

    // Editor debug: bone weight visualization (heat map overlay)
    bool showWeights = false;
    i32 weightPreviewBoneIndex = -1;  // -1 = no bone selected for preview

    // Editor: 3D skeletal onion skinning settings
    SkeletalOnionSkinSettings onionSkin;

    // Blend tree: parameter-driven animation blending
    Animation::BlendTree blendTree;
    std::unordered_map<std::string, f32> blendParameters;  // Runtime parameter values

    AnimatorComponent() = default;

    // stateMachine holds a raw `SkeletalAnimator*` to its sibling `animator` member.
    // The defaulted copy/move would propagate the source's pointer (which references
    // the SOURCE's animator), leaving us with a dangling pointer once the source is
    // destroyed — segfault on the next stateMachine call. The custom copy/move below
    // re-anchor the pointer to *this*->animator after construction/assignment.
    AnimatorComponent(const AnimatorComponent& other)
        : animator(other.animator), stateMachine(other.stateMachine),
          matricesDirty(other.matricesDirty), showBones(other.showBones),
          selectedBoneIndex(other.selectedBoneIndex), showWeights(other.showWeights),
          weightPreviewBoneIndex(other.weightPreviewBoneIndex), onionSkin(other.onionSkin),
          blendTree(other.blendTree), blendParameters(other.blendParameters) {
        stateMachine.SetAnimator(&animator);
    }

    AnimatorComponent(AnimatorComponent&& other) noexcept
        : animator(std::move(other.animator)), stateMachine(std::move(other.stateMachine)),
          matricesDirty(other.matricesDirty), showBones(other.showBones),
          selectedBoneIndex(other.selectedBoneIndex), showWeights(other.showWeights),
          weightPreviewBoneIndex(other.weightPreviewBoneIndex),
          onionSkin(std::move(other.onionSkin)), blendTree(std::move(other.blendTree)),
          blendParameters(std::move(other.blendParameters)) {
        stateMachine.SetAnimator(&animator);
    }

    AnimatorComponent& operator=(const AnimatorComponent& other) {
        if (this == &other) return *this;
        animator = other.animator;
        stateMachine = other.stateMachine;
        matricesDirty = other.matricesDirty;
        showBones = other.showBones;
        selectedBoneIndex = other.selectedBoneIndex;
        showWeights = other.showWeights;
        weightPreviewBoneIndex = other.weightPreviewBoneIndex;
        onionSkin = other.onionSkin;
        blendTree = other.blendTree;
        blendParameters = other.blendParameters;
        stateMachine.SetAnimator(&animator);
        return *this;
    }

    AnimatorComponent& operator=(AnimatorComponent&& other) noexcept {
        if (this == &other) return *this;
        animator = std::move(other.animator);
        stateMachine = std::move(other.stateMachine);
        matricesDirty = other.matricesDirty;
        showBones = other.showBones;
        selectedBoneIndex = other.selectedBoneIndex;
        showWeights = other.showWeights;
        weightPreviewBoneIndex = other.weightPreviewBoneIndex;
        onionSkin = std::move(other.onionSkin);
        blendTree = std::move(other.blendTree);
        blendParameters = std::move(other.blendParameters);
        stateMachine.SetAnimator(&animator);
        return *this;
    }

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
