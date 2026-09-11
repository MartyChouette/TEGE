#pragma once

// The walkthrough that makes Tutorial mode different from Creative mode.
//
// Tutorial IS Creative -- same rail, same tools, same gestures -- with an
// ordered set of steps running over it and a guide that says what to try next.
// Nothing here changes what the tools do. A tutorial that put you in a special
// sandbox would teach you the sandbox.
//
// The steps are DATA, and each one's completion is a predicate over the world
// and the editor's own state rather than a script the tutorial runs. That is
// the difference between a walkthrough and a cutscene: you finish a step by
// doing the thing, in any order you like within the step, and wandering off to
// build something else does not desync it.
//
// Deliberately not a checklist you tick yourself. A step that completes when
// you say it did teaches nothing, and a step that cannot detect its own
// completion has no business claiming to.

#include "Enjin/Platform/Types.h"
#include "Enjin/Platform/Platform.h"
#include "Enjin/Editor/CreativeMode.h"

#include <functional>
#include <string>
#include <vector>

namespace Enjin {
namespace ECS { class World; }

namespace Editor {

// What a step can see when it decides whether it is done. Passed by the editor
// each frame; a step never reaches into the editor itself, so the steps stay
// testable without one.
struct WalkthroughContext {
    ECS::World* world = nullptr;
    BuildTool currentTool = BuildTool::Wall;
    u32 entitiesBuiltThisSession = 0;   // entities the build rail has created
    bool isPlaying = false;
};

struct WalkthroughStep {
    // What to do, in the imperative. Shown as the step's heading.
    std::string instruction;

    // The guide's line. Says WHY, where the instruction says what -- an
    // instruction you follow without knowing why teaches a sequence rather than
    // a tool.
    std::string guidance;

    // True once the step has been done. Evaluated every frame, so it must be
    // cheap and must not mutate anything.
    std::function<bool(const WalkthroughContext&)> isComplete;
};

// The ordered steps. One list, so the guide, the progress readout and the
// completion check cannot disagree about what step you are on.
ENJIN_API const std::vector<WalkthroughStep>& DefaultWalkthrough();

// Tracks which step is current. Advances when the current step completes, and
// never goes backwards on its own: a step that un-completes because you deleted
// what you built would otherwise drag you back through the tutorial.
class ENJIN_API WalkthroughState {
public:
    void Reset();

    // Returns true on the frame a step is completed, so the caller can react
    // once -- play a sound, announce it -- rather than every frame after.
    bool Update(const WalkthroughContext& ctx);

    usize CurrentStep() const { return m_Current; }
    usize StepCount() const;
    bool IsFinished() const;

    // The step to show. Returns the LAST step when finished, so the panel has
    // something to render rather than reading off the end.
    const WalkthroughStep& Current() const;

private:
    usize m_Current = 0;
    bool m_Finished = false;
};

} // namespace Editor
} // namespace Enjin
