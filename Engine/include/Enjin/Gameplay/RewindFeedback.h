#pragma once

// The screen tint and vignette a Rewind Ability wears while it runs.
//
// RecordRewindComponent and SceneRewindComponent have carried
// rewindVignetteStrength and rewindTint since they were written. Both are
// authored in the inspector under a "Visual Feedback" heading, both are
// serialized into the scene, and the user manual describes rewindTint as a
// "screen tint". Nothing in the renderer had ever read either one -- the only
// consumer anywhere was the colour of an ImGui progress bar in an editor panel,
// which is not a thing a player can see.
//
// So a designer authoring a Sands of Time rewind picked a gold, saved it, ran the
// game, and got no gold. The failure mode is the cruel one: the feature looks
// switched off rather than missing, and the obvious next move is to turn the
// number up.
//
// This applies them to post-processing while a rewind is active and puts the
// previous values back when it ends. Edge-triggered, because writing the saved
// values back every frame would fight anything else driving the same fields.

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"

namespace Enjin {
namespace Renderer { struct PostProcessSettings; }
namespace Gameplay {

class RecordRewindSystem;

// What a rewind wants the screen to look like. Zero strength and a white tint
// mean "nothing", which is also what a component with the effect turned off
// produces -- an absence rather than a plausible default.
struct RewindFeedback {
    bool active = false;
    f32 vignetteStrength = 0.0f;
    Math::Vector3 tint = Math::Vector3(1.0f, 1.0f, 1.0f);
};

// Owned by each runtime (editor PlayMode, the native player, the web player) and
// called once per frame after the rewind system has updated. It holds the saved
// settings, so one instance per post-process stack.
class ENJIN_API RewindFeedbackApplier {
public:
    void Apply(const RecordRewindSystem& rewind, Renderer::PostProcessSettings& pp);

    // Put the screen back without needing a rewind system. Called on play stop:
    // a session that ended mid-rewind would otherwise strand the tint, and the
    // editor would carry a gold wash into edit mode with nothing to explain it.
    void Reset(Renderer::PostProcessSettings& pp);

private:
    bool m_Applied = false;
    u32 m_SavedVignetteEnabled = 0;
    f32 m_SavedVignetteIntensity = 0.0f;
    Math::Vector3 m_SavedColorFilter = Math::Vector3(1.0f, 1.0f, 1.0f);
};

} // namespace Gameplay
} // namespace Enjin
