#include "Enjin/Effects/FluidPlaybackSystem.h"
#include "Enjin/Effects/FluidSimulation.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/FluidVolume.h"
#include "Enjin/ECS/Components/FluidPlayback.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Platform/Paths.h"

#include <filesystem>

namespace Enjin {
namespace Effects {

void FluidPlaybackSystem::Update(f32 deltaTime, ECS::World* world, FluidSimulation& sim) {
    if (!world) return;

    for (ECS::Entity e : world->GetEntitiesWithComponent<ECS::FluidPlaybackComponent>()) {
        auto* play = world->GetComponent<ECS::FluidPlaybackComponent>(e);
        if (!play) continue;

        // The volume carries the extents and the look. A playback component on
        // its own has nothing to drive.
        auto* vol = world->GetComponent<ECS::FluidVolumeComponent>(e);
        if (!vol || !vol->isActive) continue;

        if (play->bakePath.empty()) continue;   // nothing baked yet; not an error

        // Read once. A remembered failure is what stops a missing recording
        // being re-opened from disk sixty times a second, and what stops the
        // log filling with the same line.
        std::shared_ptr<FluidBake> bake;
        auto cached = m_Cache.find(play->bakePath);
        if (cached != m_Cache.end()) {
            bake = cached->second;
        } else if (!play->loadAttempted) {
            play->loadAttempted = true;

            // Project-relative, resolved inside the asset root. The CWD is
            // never reliable (editor and player both run from their exe
            // directory), and a path that escapes the root is refused rather
            // than followed.
            std::string resolved = play->bakePath;
            if (!m_AssetRoot.empty()) {
                resolved = Platform::ResolveWithinRoot(m_AssetRoot, play->bakePath);
                if (resolved.empty()) {
                    ENJIN_LOG_ERROR(Build, "Fluid playback: '%s' resolves outside the asset root",
                                    play->bakePath.c_str());
                    play->loadFailed = true;
                    m_Cache[play->bakePath] = nullptr;
                    continue;
                }
            }

            auto loaded = std::make_shared<FluidBake>();
            if (loaded->Load(resolved) && loaded->FrameCount() > 0) {
                m_Cache[play->bakePath] = loaded;
                bake = loaded;
            } else {
                ENJIN_LOG_ERROR(Build, "Fluid playback: cannot read recording '%s'",
                                play->bakePath.c_str());
                play->loadFailed = true;
                m_Cache[play->bakePath] = nullptr;   // remembered failure
                continue;
            }
        } else {
            continue;   // already tried and failed
        }

        if (!bake) continue;

        if (play->playing) play->time += deltaTime * play->speed;

        if (!bake->SampleAt(play->time, m_Scratch)) continue;

        // Handing the decoded frame to the simulation rather than to the
        // renderer is what makes every existing consumer work unchanged: the
        // Vulkan renderer, the web sprite path and BuildFluidSurface all read
        // the grid, and none of them needs to know nothing solved it.
        sim.SetPlaybackDensity(e, bake->gridSize, bake->is3D, m_Scratch);
    }
}

} // namespace Effects
} // namespace Enjin
