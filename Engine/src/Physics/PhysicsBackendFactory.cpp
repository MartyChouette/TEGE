#include "Enjin/Physics/PhysicsBackendFactory.h"
#include "Enjin/Logging/Log.h"

#ifdef ENJIN_PHYSICS_SIMPLE
#include "Enjin/Physics/SimplePhysicsBackend.h"
#include "Enjin/Physics/SimplePhysicsBackend2D.h"
#endif

#ifdef ENJIN_PHYSICS_JOLT
#include "Enjin/Physics/JoltBackend.h"
#endif

#ifdef ENJIN_PHYSICS_BOX2D
#include "Enjin/Physics/Box2DBackend.h"
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
    if (type == PhysicsBackendType::Jolt) {
        ENJIN_LOG_WARN(Physics, "Jolt Physics requested but ENJIN_PHYSICS_JOLT is not enabled");
    }
#endif

    // Explicit Simple request or fallback
#ifdef ENJIN_PHYSICS_SIMPLE
    if (type == PhysicsBackendType::Simple || type == PhysicsBackendType::Auto) {
        if (type == PhysicsBackendType::Auto) {
            ENJIN_LOG_WARN(Physics, "No production 3D backend matched for Auto — falling back to SimplePhysics");
        }
        auto backend = std::make_unique<SimplePhysicsBackend>();
        ENJIN_LOG_INFO(Physics, "Created physics backend: %s", backend->GetName());
        return backend;
    }
    // Box2D requested for 3D backend — dimension mismatch, fall back
    if (type == PhysicsBackendType::Box2D) {
        ENJIN_LOG_WARN(Physics, "Box2D is a 2D backend — cannot use for 3D physics, falling back to SimplePhysics");
        auto backend = std::make_unique<SimplePhysicsBackend>();
        ENJIN_LOG_INFO(Physics, "Created physics backend: %s", backend->GetName());
        return backend;
    }
#else
    ENJIN_LOG_ERROR(Physics, "No 3D physics backend available (ENJIN_PHYSICS_SIMPLE=OFF, Jolt not matched)");
#endif

    return nullptr;
}

std::unique_ptr<IPhysicsBackend2D> CreatePhysicsBackend2D(PhysicsBackendType type, Scene::ProjectMode mode) {
#ifdef ENJIN_PHYSICS_BOX2D
    // Use Box2D when explicitly requested, or when Auto-selecting for 2D/Mixed modes
    bool useBox2D = (type == PhysicsBackendType::Box2D) ||
                    (type == PhysicsBackendType::Auto &&
                     (mode == Scene::ProjectMode::Mode2D || mode == Scene::ProjectMode::Mixed));

    if (useBox2D) {
        auto backend = std::make_unique<Box2DBackend>();
        ENJIN_LOG_INFO(Physics, "Created 2D physics backend: %s", backend->GetName());
        return backend;
    }
#else
    if (type == PhysicsBackendType::Box2D) {
        ENJIN_LOG_WARN(Physics, "Box2D requested but ENJIN_PHYSICS_BOX2D is not enabled");
    }
#endif

    // Explicit Simple request or fallback
#ifdef ENJIN_PHYSICS_SIMPLE
    if (type == PhysicsBackendType::Simple || type == PhysicsBackendType::Auto) {
        if (type == PhysicsBackendType::Auto) {
            ENJIN_LOG_WARN(Physics, "No production 2D backend matched for Auto — falling back to SimplePhysics2D");
        }
        auto backend = std::make_unique<SimplePhysicsBackend2D>();
        ENJIN_LOG_INFO(Physics, "Created 2D physics backend: %s", backend->GetName());
        return backend;
    }
    // Jolt requested for 2D backend — dimension mismatch, fall back
    if (type == PhysicsBackendType::Jolt) {
        ENJIN_LOG_WARN(Physics, "Jolt is a 3D backend — cannot use for 2D physics, falling back to SimplePhysics2D");
        auto backend = std::make_unique<SimplePhysicsBackend2D>();
        ENJIN_LOG_INFO(Physics, "Created 2D physics backend: %s", backend->GetName());
        return backend;
    }
#else
    ENJIN_LOG_ERROR(Physics, "No 2D physics backend available (ENJIN_PHYSICS_SIMPLE=OFF, Box2D not matched)");
#endif

    return nullptr;
}

const char* ResolveBackendName(PhysicsBackendType type, Scene::ProjectMode mode) {
    switch (type) {
        case PhysicsBackendType::Jolt:
#ifdef ENJIN_PHYSICS_JOLT
            return "Jolt Physics";
#else
            return "Jolt Physics (not compiled)";
#endif
        case PhysicsBackendType::Box2D:
#ifdef ENJIN_PHYSICS_BOX2D
            return "Box2D";
#else
            return "Box2D (not compiled)";
#endif
        case PhysicsBackendType::Simple:
#ifdef ENJIN_PHYSICS_SIMPLE
            return "SimplePhysics (Legacy)";
#else
            return "SimplePhysics (not compiled)";
#endif
        case PhysicsBackendType::Auto:
        default:
#ifdef ENJIN_PHYSICS_JOLT
            if (mode == Scene::ProjectMode::Mode3D || mode == Scene::ProjectMode::Mixed)
                return "Jolt Physics";
#endif
#ifdef ENJIN_PHYSICS_BOX2D
            if (mode == Scene::ProjectMode::Mode2D || mode == Scene::ProjectMode::Mixed)
                return "Box2D";
#endif
#ifdef ENJIN_PHYSICS_SIMPLE
            return "SimplePhysics (fallback)";
#else
            return "None available";
#endif
    }
}

} // namespace Physics
} // namespace Enjin
