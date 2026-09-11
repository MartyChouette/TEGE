#include "Enjin/Editor/Walkthrough.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Light.h"
#include "Enjin/ECS/Components/WaterVolume.h"
#include "Enjin/ECS/Components/Water3D.h"
#include "Enjin/ECS/Components/GrassVolume.h"
#include "Enjin/ECS/Components/ShrubVolume.h"
#include "Enjin/ECS/Components/TreeVolume.h"

namespace Enjin::Editor {

namespace {

// Counts entities carrying a component, which is how most steps tell whether
// the thing they asked for exists. Cheap: these are dense storages and the
// tutorial only runs while the tutorial is up.
template <typename T>
usize CountWith(ECS::World* world) {
    if (!world) return 0;
    return world->GetEntitiesWithComponent<T>().size();
}

} // namespace

const std::vector<WalkthroughStep>& DefaultWalkthrough() {
    // Built once. The steps hold std::function, so rebuilding this list per
    // frame would allocate a dozen closures a frame for no reason.
    static const std::vector<WalkthroughStep> kSteps = [] {
        std::vector<WalkthroughStep> v;

        v.push_back({
            "Pick the Wall tool",
            "Everything on the left rail is one tool. Only one is active at a "
            "time, so there is never a question about what a drag will do.",
            [](const WalkthroughContext& c) { return c.currentTool == BuildTool::Wall; }
        });

        v.push_back({
            "Drag out a wall",
            "Press on the ground, drag, release. The height and thickness above "
            "stay put between walls, so the next one matches without asking.",
            [](const WalkthroughContext& c) {
                return CountWith<ECS::BrushSolidComponent>(c.world) > 0;
            }
        });

        v.push_back({
            "Put a floor down",
            "Floor snaps to the walls already there. Build the room, then the "
            "floor, and the two meet without you aligning anything.",
            [](const WalkthroughContext& c) { return c.currentTool == BuildTool::Floor; }
        });

        v.push_back({
            "Add some water",
            "Water is swimmable by default -- that is a body with depth, not a "
            "painted surface, and a character will swim in it.",
            [](const WalkthroughContext& c) {
                return CountWith<ECS::WaterVolumeComponent>(c.world) > 0 ||
                       CountWith<ECS::Water3DComponent>(c.world) > 0;
            }
        });

        v.push_back({
            "Plant something",
            "Grass, shrubs or trees, sized by the drag. The Density setting is "
            "how thick, not how big -- the drag decides how big.",
            [](const WalkthroughContext& c) {
                return CountWith<ECS::GrassVolumeComponent>(c.world) > 0 ||
                       CountWith<ECS::ShrubVolumeComponent>(c.world) > 0 ||
                       CountWith<ECS::TreeVolumeComponent>(c.world) > 0;
            }
        });

        v.push_back({
            "Drop in a light",
            "The Prop tool places ready-made things. Kind picks which: a ball, "
            "a light, a physics box, a barrel, a spawn point.",
            [](const WalkthroughContext& c) {
                return CountWith<ECS::LightComponent>(c.world) > 0;
            }
        });

        v.push_back({
            "Press Play",
            "The same scene, running. Nothing is exported or rebuilt -- what you "
            "just built is what you are standing in.",
            [](const WalkthroughContext& c) { return c.isPlaying; }
        });

        return v;
    }();

    return kSteps;
}

void WalkthroughState::Reset() {
    m_Current = 0;
    m_Finished = false;
}

usize WalkthroughState::StepCount() const {
    return DefaultWalkthrough().size();
}

bool WalkthroughState::IsFinished() const {
    return m_Finished;
}

const WalkthroughStep& WalkthroughState::Current() const {
    const auto& steps = DefaultWalkthrough();
    // Clamped rather than indexed off the end: when the walkthrough is finished
    // the panel still wants something to show.
    const usize i = (m_Current < steps.size()) ? m_Current : steps.size() - 1;
    return steps[i];
}

bool WalkthroughState::Update(const WalkthroughContext& ctx) {
    if (m_Finished) return false;

    const auto& steps = DefaultWalkthrough();
    if (steps.empty()) {
        m_Finished = true;
        return false;
    }
    if (m_Current >= steps.size()) {
        m_Finished = true;
        return false;
    }

    const auto& step = steps[m_Current];
    if (!step.isComplete || !step.isComplete(ctx)) return false;

    // Forward only. A step whose condition stops holding -- you deleted the wall
    // you built -- must not drag you back through the tutorial, because the
    // point was that you had done it once.
    ++m_Current;
    if (m_Current >= steps.size()) m_Finished = true;
    return true;
}

} // namespace Enjin::Editor
