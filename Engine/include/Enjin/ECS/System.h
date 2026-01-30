#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Core/Concepts.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/ECS/Component.h"
#include <concepts>
#include <vector>
#include <memory>

namespace Enjin {
namespace ECS {

// Forward declaration
class World;

// Base system interface
class ENJIN_API ISystem {
public:
    virtual ~ISystem() = default;
    virtual void Update(f32 deltaTime) = 0;
    virtual void OnEntityAdded(Entity entity) {}
    virtual void OnEntityRemoved(Entity entity) {}
};

// System manager - manages all systems
class ENJIN_API SystemManager {
public:
    SystemManager();
    ~SystemManager();

    // Register a system — T must satisfy IsSystem (has Update(f32)) and derive from ISystem
    template<typename T, typename... Args>
        requires IsSystem<T> && std::derived_from<T, ISystem>
    T* RegisterSystem(Args&&... args) {
        auto system = std::make_unique<T>(std::forward<Args>(args)...);
        T* ptr = system.get();
        m_Systems.push_back(std::move(system));
        return ptr;
    }

    void Update(f32 deltaTime);
    void OnEntityAdded(Entity entity);
    void OnEntityRemoved(Entity entity);

private:
    std::vector<std::unique_ptr<ISystem>> m_Systems;
};

} // namespace ECS
} // namespace Enjin
