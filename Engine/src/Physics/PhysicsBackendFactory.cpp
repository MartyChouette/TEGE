#include "Enjin/Physics/PhysicsBackendFactory.h"
#include "Enjin/Physics/SimplePhysicsBackend.h"
#include "Enjin/Physics/SimplePhysicsBackend2D.h"
#include "Enjin/Logging/Log.h"

#ifdef ENJIN_PHYSICS_JOLT
#include "Enjin/Physics/JoltBackend.h"
#endif

namespace Enjin {
namespace Physics {

std::unique_ptr<IPhysicsBackend> CreatePhysicsBackend(PhysicsBackendType type, Scene::ProjectMode mode) {
#ifdef ENJIN_PHYSICS_JOLT
    // Use Jolt when explicitly requested, or when Auto-selecting for 3D/Mixed modes
    bool useJolt = (type == PhysicsBackendType::Jolt) ||
                   (type == PhysicsBackendType::Auto &&
                    (mode == Scene::ProjectMode::Mode3D || mode == Scene::ProjectMode::Mixed));

    if (useJolt) {
        auto backend = std::make_unique<JoltBackend>();
        ENJIN_LOG_INFO(Physics, "Created physics backend: %s", backend->GetName());
        return backend;
    }
#else
    (void)type;
    (void)mode;
#endif

    auto backend = std::make_unique<SimplePhysicsBackend>();
    ENJIN_LOG_INFO(Physics, "Created physics backend: %s", backend->GetName());
    return backend;
}

std::unique_ptr<IPhysicsBackend2D> CreatePhysicsBackend2D(PhysicsBackendType type, Scene::ProjectMode mode) {
    // Phase 3 will add: if Box2D requested/available, return Box2DBackend
    (void)type;
    (void)mode;

    auto backend = std::make_unique<SimplePhysicsBackend2D>();
    ENJIN_LOG_INFO(Physics, "Created 2D physics backend: %s", backend->GetName());
    return backend;
}

} // namespace Physics
} // namespace Enjin
