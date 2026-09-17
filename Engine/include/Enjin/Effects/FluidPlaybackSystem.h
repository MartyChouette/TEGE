#pragma once
// Drives fluid volumes from recorded simulations.
//
// Runs in place of the solver for any volume carrying a
// FluidPlaybackComponent: advance the playback head, decode that frame, hand
// it to FluidSimulation as playback density. Everything downstream -- the
// Vulkan FluidRenderer, the web sprite path, BuildFluidSurface -- reads the
// grid and neither knows nor cares that nothing solved it.
//
// Recordings are cached by path and shared: twenty chimneys playing the same
// take hold one copy of it, which is the difference between 7 MB and 140 MB
// for a town.
#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Effects/FluidBake.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Enjin {
namespace ECS { class World; }
namespace Effects {

class FluidSimulation;

class ENJIN_API FluidPlaybackSystem {
public:
    // `assetRoot` is what a project-relative bakePath resolves against. The
    // process CWD is never reliable here (the editor and player both run from
    // their exe directory), so this is set at play/boot like every other root
    // in the engine.
    void SetAssetRoot(const std::string& root) { m_AssetRoot = root; }

    void Update(f32 deltaTime, ECS::World* world, FluidSimulation& sim);

    // Drop every cached recording. Call on scene change: a cache keyed by path
    // would otherwise hold a previous level's takes alive for the session.
    void ClearCache() { m_Cache.clear(); }

    usize CachedBakeCount() const { return m_Cache.size(); }

private:
    // Shared, so N volumes playing one take hold one copy. A null entry is a
    // remembered FAILURE -- the path was tried and did not load -- which stops
    // a missing file being re-read from disk every frame.
    std::unordered_map<std::string, std::shared_ptr<FluidBake>> m_Cache;
    std::string m_AssetRoot;
    std::vector<f32> m_Scratch;   // decode target, reused across volumes
};

} // namespace Effects
} // namespace Enjin
