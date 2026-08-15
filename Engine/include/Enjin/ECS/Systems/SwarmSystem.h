#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"

namespace Enjin {
namespace ECS {

class World;

// Ticks every SwarmComponent in play mode: lazily builds the SoA sim + a pool
// of proxy cube entities, advances the sim, and copies positions onto the
// proxies. Reset() tears the proxies down (call on play-stop).
class ENJIN_API SwarmSystem {
public:
    void Update(World* world, f32 deltaTime);
    void Reset(World* world);   // destroy all proxy entities, allow respawn

    void SetEnabled(bool enabled) { m_Enabled = enabled; }
    bool IsEnabled() const { return m_Enabled; }

private:
    bool m_Enabled = true;
};

} // namespace ECS
} // namespace Enjin
