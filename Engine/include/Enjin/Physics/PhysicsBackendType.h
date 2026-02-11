#pragma once

#include "Enjin/Platform/Platform.h"
#include <cstdint>

namespace Enjin {
namespace Physics {

// Which physics implementation to use
enum class PhysicsBackendType : std::uint8_t {
    Auto,       // Auto-select based on ProjectMode (Jolt for 3D/Mixed, Box2D for 2D)
    Jolt,       // Jolt Physics (3D)
    Box2D       // Box2D v3 (2D)
};

} // namespace Physics
} // namespace Enjin
