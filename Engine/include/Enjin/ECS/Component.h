#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/Entity.h"
#include <atomic>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace Enjin {
namespace ECS {

// Component type ID
using ComponentTypeId = u32;

// Base component interface
struct ENJIN_API IComponent {
    virtual ~IComponent() = default;
};

// Component registry - manages component type IDs
class ENJIN_API ComponentRegistry {
public:
    template<typename T>
    static ComponentTypeId GetTypeId() {
        static ComponentTypeId id = s_NextComponentId.fetch_add(1, std::memory_order_relaxed);
        return id;
    }

    static ComponentTypeId GetNextId() { return s_NextComponentId.load(std::memory_order_relaxed); }

private:
    static std::atomic<ComponentTypeId> s_NextComponentId;
};

// Component storage - Structure of Arrays (SoA) for cache efficiency
template<typename T>
class ComponentStorage {
public:
    T& Add(Entity entity) {
        m_Entities.push_back(entity);
        m_Components.push_back(T{});
        m_EntityToIndex[entity] = m_Components.size() - 1;
        return m_Components.back();
    }

    void Remove(Entity entity) {
        auto it = m_EntityToIndex.find(entity);
        if (it == m_EntityToIndex.end()) {
            return;
        }

        usize index = it->second;
        usize lastIndex = m_Components.size() - 1;

        // Swap with last element
        if (index != lastIndex) {
            m_Components[index] = std::move(m_Components[lastIndex]);
            m_Entities[index] = m_Entities[lastIndex];
            m_EntityToIndex[m_Entities[index]] = index;
        }

        m_Components.pop_back();
        m_Entities.pop_back();
        m_EntityToIndex.erase(it);
    }

    T* Get(Entity entity) {
        auto it = m_EntityToIndex.find(entity);
        if (it == m_EntityToIndex.end()) {
            return nullptr;
        }
        return &m_Components[it->second];
    }

    const T* Get(Entity entity) const {
        auto it = m_EntityToIndex.find(entity);
        if (it == m_EntityToIndex.end()) {
            return nullptr;
        }
        return &m_Components[it->second];
    }

    bool Has(Entity entity) const {
        return m_EntityToIndex.find(entity) != m_EntityToIndex.end();
    }

    void Clear() {
        m_Components.clear();
        m_Entities.clear();
        m_EntityToIndex.clear();
    }

    usize Size() const { return m_Components.size(); }
    bool Empty() const { return m_Components.empty(); }

    // Iteration support
    const std::vector<Entity>& GetEntities() const { return m_Entities; }
    const std::vector<T>& GetComponents() const { return m_Components; }
    std::vector<T>& GetComponents() { return m_Components; }

private:
    std::vector<Entity> m_Entities;
    std::vector<T> m_Components;
    std::unordered_map<Entity, usize> m_EntityToIndex;
};

} // namespace ECS
} // namespace Enjin
