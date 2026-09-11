#include "Enjin/Gameplay/RewindFeedback.h"
#include "Enjin/Gameplay/RecordRewindSystem.h"
#include "Enjin/Renderer/PostProcessing.h"
#include "Enjin/Math/Math.h"

namespace Enjin::Gameplay {

void RewindFeedbackApplier::Apply(const RecordRewindSystem& rewind,
                                  Renderer::PostProcessSettings& pp) {
    const RewindFeedback want = rewind.GetActiveFeedback();

    if (want.active) {
        if (!m_Applied) {
            // Save once, on the rising edge. Saving every frame would capture the
            // values this function itself just wrote, and the restore would then
            // put the rewind look back rather than what the player had.
            m_SavedVignetteEnabled = pp.vignetteEnabled;
            m_SavedVignetteIntensity = pp.vignetteIntensity;
            m_SavedColorFilter = pp.colorFilter;
            m_Applied = true;
        }

        if (want.vignetteStrength > 0.0001f) {
            pp.vignetteEnabled = 1;
            // Take the stronger of the two rather than replacing: a game that ships
            // a vignette already should not LOSE it during a rewind.
            pp.vignetteIntensity = Math::Max(m_SavedVignetteIntensity, want.vignetteStrength);
        }
        // Multiply into whatever grade is already there, so a rewind tints the
        // game's look instead of discarding it.
        pp.colorFilter = Math::Vector3(m_SavedColorFilter.x * want.tint.x,
                                       m_SavedColorFilter.y * want.tint.y,
                                       m_SavedColorFilter.z * want.tint.z);
        return;
    }

    if (m_Applied) Reset(pp);
}

void RewindFeedbackApplier::Reset(Renderer::PostProcessSettings& pp) {
    if (!m_Applied) return;
    pp.vignetteEnabled = m_SavedVignetteEnabled;
    pp.vignetteIntensity = m_SavedVignetteIntensity;
    pp.colorFilter = m_SavedColorFilter;
    m_Applied = false;
}

} // namespace Enjin::Gameplay
