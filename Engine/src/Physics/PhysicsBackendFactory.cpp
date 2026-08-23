#include "Enjin/Platform/Platform.h"
#include "Enjin/Physics/PhysicsBackendFactory.h"
#include "Enjin/Logging/Log.h"

// Both Jolt Physics and Box2D v3 compile under Emscripten (WASM).
// No special handling needed — the backends work identically on web.

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

    if (type == PhysicsBackendType::Box2D) {
        ENJIN_LOG_WARN(Physics, "Box2D is a 2D backend — cannot use for 3D physics");
    }

    // Auto in a 2D project goes without a 3D backend on purpose (mirrors the
    // 2D factory below) - only an unfulfillable request is worth an error.
    if (type != PhysicsBackendType::Auto) {
        ENJIN_LOG_ERROR(Physics, "No 3D physics backend available");
    }
#ifndef ENJIN_PHYSICS_JOLT
    else if (mode == Scene::ProjectMode::Mode3D || mode == Scene::ProjectMode::Mixed) {
        ENJIN_LOG_ERROR(Physics, "3D project, but Jolt is not compiled in — no 3D physics backend");
    }
#endif
    return nullptr;
}

std::unique_ptr<IPhysicsBackend2D> CreatePhysicsBackend2D(PhysicsBackendType type, Scene::ProjectMode mode) {
#ifdef ENJIN_PHYSICS_BOX2D
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

    if (type == PhysicsBackendType::Jolt) {
        ENJIN_LOG_WARN(Physics, "Jolt is a 3D backend — cannot use for 2D physics");
    }

    if (type != PhysicsBackendType::Auto) {
        ENJIN_LOG_ERROR(Physics, "No 2D physics backend available");
    }
#ifndef ENJIN_PHYSICS_BOX2D
    else if (mode == Scene::ProjectMode::Mode2D || mode == Scene::ProjectMode::Mixed) {
        ENJIN_LOG_ERROR(Physics, "2D project, but Box2D is not compiled in — no 2D physics backend");
    }
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
            return "None available";
    }
}

} // namespace Physics
} // namespace Enjin
