#pragma once

#include "Enjin/Physics/PhysicsBackendType.h"
#include "Enjin/Physics/IPhysicsBackend.h"
#include "Enjin/Physics/IPhysicsBackend2D.h"
#include "Enjin/Scene/SceneManager.h"  // For ProjectMode
#include <memory>

namespace Enjin {
namespace Physics {

// Create a 3D physics backend based on type and project mode.
// Auto selects Jolt for 3D/Mixed.
ENJIN_API std::unique_ptr<IPhysicsBackend> CreatePhysicsBackend(
    PhysicsBackendType type = PhysicsBackendType::Auto,
    Scene::ProjectMode mode = Scene::ProjectMode::Mode3D);

// Create a 2D physics backend based on type and project mode.
// Auto selects Box2D for 2D/Mixed.
ENJIN_API std::unique_ptr<IPhysicsBackend2D> CreatePhysicsBackend2D(
    PhysicsBackendType type = PhysicsBackendType::Auto,
    Scene::ProjectMode mode = Scene::ProjectMode::Mode2D);

// Query compile-time backend availability
inline bool IsJoltAvailable() {
#ifdef ENJIN_PHYSICS_JOLT
    return true;
#else
    return false;
#endif
}

inline bool IsBox2DAvailable() {
#ifdef ENJIN_PHYSICS_BOX2D
    return true;
#else
    return false;
#endif
}

// Get human-readable name for what Auto resolves to given current project mode
ENJIN_API const char* ResolveBackendName(PhysicsBackendType type, Scene::ProjectMode mode);

} // namespace Physics
} // namespace Enjin
