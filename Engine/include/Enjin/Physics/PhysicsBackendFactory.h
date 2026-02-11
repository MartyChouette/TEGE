#pragma once

#include "Enjin/Physics/PhysicsBackendType.h"
#include "Enjin/Physics/IPhysicsBackend.h"
#include "Enjin/Physics/IPhysicsBackend2D.h"
#include "Enjin/Scene/SceneManager.h"  // For ProjectMode
#include <memory>

namespace Enjin {
namespace Physics {

// Create a 3D physics backend based on type and project mode.
// For now always returns SimplePhysicsBackend; Jolt backend added in Phase 2.
ENJIN_API std::unique_ptr<IPhysicsBackend> CreatePhysicsBackend(
    PhysicsBackendType type = PhysicsBackendType::Auto,
    Scene::ProjectMode mode = Scene::ProjectMode::Mode3D);

// Create a 2D physics backend based on type and project mode.
// For now always returns SimplePhysicsBackend2D; Box2D backend added in Phase 3.
ENJIN_API std::unique_ptr<IPhysicsBackend2D> CreatePhysicsBackend2D(
    PhysicsBackendType type = PhysicsBackendType::Auto,
    Scene::ProjectMode mode = Scene::ProjectMode::Mode2D);

} // namespace Physics
} // namespace Enjin
