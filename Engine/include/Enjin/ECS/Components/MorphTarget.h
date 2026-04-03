#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/ECS/Entity.h"
#include <string>
#include <vector>

namespace Enjin::ECS {

// Per-vertex delta for a single morph target
struct MorphTargetDelta {
    Math::Vector3 positionDelta;
    Math::Vector3 normalDelta;
};

// A single named morph target (e.g., "smile", "blink_L", "jawOpen")
struct MorphTarget {
    std::string name;
    std::vector<MorphTargetDelta> deltas;  // One per vertex in base mesh
};

// MorphTargetComponent — stores blend shape data for mesh deformation.
// Imported from glTF/FBX. Weights driven by animation, script, or LipSync.
struct MorphTargetComponent {
    std::vector<MorphTarget> targets;   // All named targets
    std::vector<f32> weights;           // Current weight per target [0..1]
    bool weightsDirty = true;           // Triggers GPU SSBO re-upload
    bool deltasDirty = true;            // Full SSBO re-upload needed (first frame or data change)

    i32 FindTarget(const std::string& name) const {
        for (usize i = 0; i < targets.size(); ++i) {
            if (targets[i].name == name) return static_cast<i32>(i);
        }
        return -1;
    }

    void SetWeight(const std::string& name, f32 weight) {
        i32 idx = FindTarget(name);
        if (idx >= 0 && idx < static_cast<i32>(weights.size())) {
            if (weights[idx] != weight) {
                weights[idx] = weight;
                weightsDirty = true;
            }
        }
    }

    void SetWeight(i32 index, f32 weight) {
        if (index >= 0 && index < static_cast<i32>(weights.size())) {
            if (weights[index] != weight) {
                weights[index] = weight;
                weightsDirty = true;
            }
        }
    }

    f32 GetWeight(const std::string& name) const {
        i32 idx = FindTarget(name);
        return (idx >= 0 && idx < static_cast<i32>(weights.size())) ? weights[idx] : 0.0f;
    }
};

} // namespace Enjin::ECS
