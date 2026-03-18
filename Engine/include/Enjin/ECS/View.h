#pragma once

#include "Enjin/ECS/Entity.h"
#include "Enjin/ECS/Component.h"
#include <algorithm>
#include <array>
#include <tuple>

/**
 * @file View.h
 * @brief Lightweight multi-component view for efficient ECS iteration
 *
 * Avoids redundant type-ID hash map lookups by caching ComponentStorage<T>*
 * pointers up front, then iterating the smallest component set and filtering
 * against all others.
 *
 * Usage:
 *   ECS::View<TransformComponent, MeshComponent, MaterialComponent> view(world);
 *   for (Entity e : view) {
 *       auto* t = view.Get<TransformComponent>(e);   // direct storage lookup
 *       auto* m = view.Get<MeshComponent>(e);
 *       auto* mat = view.Get<MaterialComponent>(e);   // guaranteed non-null
 *   }
 *
 * The view iterates the component set with the fewest entities and skips any
 * entity that is missing from the other sets. All returned entities are
 * guaranteed to have every requested component type.
 *
 * Exclude filter (optional):
 *   ECS::View<TransformComponent, MeshComponent> view(world);
 *   view.Exclude<Sprite2DComponent>();   // cache the exclusion storage
 *   // Iteration now skips entities that have Sprite2DComponent
 *
 * No heap allocations. Header-only. Fully inline.
 */

namespace Enjin {

// Forward declaration — full definition lives in World.h
namespace ECS { class World; }

namespace ECS {

// ─────────────────────────────────────────────────────────────────────────────
// View — iterates entities that have ALL of the listed component types
// ─────────────────────────────────────────────────────────────────────────────
template<typename... Components>
class View {
    static constexpr usize N = sizeof...(Components);
    static_assert(N >= 1, "View requires at least one component type");

public:
    // ── Construction ─────────────────────────────────────────────────────
    // World is forward-declared here; callers must include World.h before
    // instantiating a View (which is always the case in practice).
    explicit View(World& world)
        : m_Storages{ world.GetComponentStorage<Components>()... }
    {
        // Find the storage with the fewest entities (pivot set).
        m_SmallestIndex = 0;
        usize smallestSize = StorageSize(0);
        for (usize i = 1; i < N; ++i) {
            usize sz = StorageSize(i);
            if (sz < smallestSize) {
                smallestSize = sz;
                m_SmallestIndex = i;
            }
        }
    }

    // ── Exclude filter ──────────────────────────────────────────────────
    // Call before iteration. Entities that HAVE the excluded component are
    // skipped. Up to 4 exclude types are supported (stack-allocated).
    template<typename Excluded>
    View& Exclude(World& world) {
        if (m_ExcludeCount < MaxExcludes) {
            m_ExcludeStorages[m_ExcludeCount++] = world.GetComponentStorage<Excluded>();
        }
        return *this;
    }

    // ── Accessors ────────────────────────────────────────────────────────
    // Get<T>(entity) — returns T* from the cached storage (no type-ID hash).
    // Undefined behaviour if T is not one of the View's component types.
    template<typename T>
    T* Get(Entity entity) const {
        auto* storage = std::get<ComponentStorage<T>*>(m_Storages);
        return storage ? storage->Get(entity) : nullptr;
    }

    template<typename T>
    bool Has(Entity entity) const {
        auto* storage = std::get<ComponentStorage<T>*>(m_Storages);
        return storage && storage->Has(entity);
    }

    // ── Iteration ────────────────────────────────────────────────────────
    class Iterator {
    public:
        Iterator(const View* view, usize index)
            : m_View(view), m_Index(index)
        {
            // Advance to first valid entity
            if (m_View) AdvanceToValid();
        }

        Entity operator*() const {
            return m_View->SmallestEntities()[m_Index];
        }

        Iterator& operator++() {
            ++m_Index;
            AdvanceToValid();
            return *this;
        }

        bool operator!=(const Iterator& other) const {
            return m_Index != other.m_Index;
        }

    private:
        void AdvanceToValid() {
            const auto& entities = m_View->SmallestEntities();
            const usize count = entities.size();
            while (m_Index < count) {
                Entity e = entities[m_Index];
                if (m_View->PassesAllFilters(e))
                    break;
                ++m_Index;
            }
        }

        const View* m_View;
        usize m_Index;
    };

    Iterator begin() const {
        if (AnyStorageNull()) return end();
        return Iterator(this, 0);
    }

    Iterator end() const {
        if (AnyStorageNull()) return Iterator(nullptr, 0);
        return Iterator(nullptr, SmallestEntities().size());
    }

    // Number of entities in the smallest component set (upper bound on
    // the number of entities the view will yield).
    usize UpperBound() const {
        if (AnyStorageNull()) return 0;
        return SmallestEntities().size();
    }

private:
    // ── Helpers ──────────────────────────────────────────────────────────
    // The tuple stores one ComponentStorage<T>* per requested type.
    using StorageTuple = std::tuple<ComponentStorage<Components>*...>;
    StorageTuple m_Storages;

    usize m_SmallestIndex = 0;

    // Exclude filter (up to 4 excluded types, stack-allocated)
    static constexpr usize MaxExcludes = 4;
    // We use void* and a type-erased Has check to avoid template bloat.
    // StorageBase (in World.h) has a virtual Has(), but it's not accessible
    // from here. Instead we store a typed lambda capture via function pointer.
    struct ExcludeEntry {
        const void* storage = nullptr;
        bool (*hasFunc)(const void*, Entity) = nullptr;
    };
    std::array<ExcludeEntry, MaxExcludes> m_ExcludeStorages{};
    usize m_ExcludeCount = 0;

    // Overload of Exclude that stores the type-erased check
public:
    template<typename Excluded>
    View& Exclude(const ComponentStorage<Excluded>* storage) {
        if (m_ExcludeCount < MaxExcludes && storage) {
            m_ExcludeStorages[m_ExcludeCount].storage = storage;
            m_ExcludeStorages[m_ExcludeCount].hasFunc = [](const void* s, Entity e) -> bool {
                return static_cast<const ComponentStorage<Excluded>*>(s)->Has(e);
            };
            m_ExcludeCount++;
        }
        return *this;
    }

private:
    // Get entity count from the i-th storage (index into the tuple at runtime).
    // We build a static dispatch table so we can index by m_SmallestIndex.
    usize StorageSize(usize index) const {
        static constexpr auto sizeGetters = MakeSizeGetters(std::index_sequence_for<Components...>{});
        if (sizeGetters[index] == nullptr) return 0;
        return sizeGetters[index](this);
    }

    using SizeGetter = usize(*)(const View*);

    template<usize... Is>
    static constexpr std::array<SizeGetter, N> MakeSizeGetters(std::index_sequence<Is...>) {
        return {{ &GetSizeAt<Is>... }};
    }

    template<usize I>
    static usize GetSizeAt(const View* self) {
        auto* s = std::get<I>(self->m_Storages);
        return s ? s->Size() : 0;
    }

    // Get the entity list from the smallest storage.
    const std::vector<Entity>& SmallestEntities() const {
        static constexpr auto entityGetters = MakeEntityGetters(std::index_sequence_for<Components...>{});
        return entityGetters[m_SmallestIndex](this);
    }

    using EntityGetter = const std::vector<Entity>&(*)(const View*);

    template<usize... Is>
    static constexpr std::array<EntityGetter, N> MakeEntityGetters(std::index_sequence<Is...>) {
        return {{ &GetEntitiesAt<Is>... }};
    }

    template<usize I>
    static const std::vector<Entity>& GetEntitiesAt(const View* self) {
        auto* s = std::get<I>(self->m_Storages);
        if (s) return s->GetEntities();
        static const std::vector<Entity> empty;
        return empty;
    }

    // Check if entity exists in ALL storages (other than the pivot).
    bool PassesAllFilters(Entity entity) const {
        // Check include filters (must have all components)
        if (!HasInAll(entity, std::index_sequence_for<Components...>{}))
            return false;
        // Check exclude filters (must not have any excluded component)
        for (usize i = 0; i < m_ExcludeCount; ++i) {
            if (m_ExcludeStorages[i].hasFunc(m_ExcludeStorages[i].storage, entity))
                return false;
        }
        return true;
    }

    template<usize... Is>
    bool HasInAll(Entity entity, std::index_sequence<Is...>) const {
        return (HasAt<Is>(entity) && ...);
    }

    template<usize I>
    bool HasAt(Entity entity) const {
        if (I == m_SmallestIndex) return true; // Already in pivot set
        auto* s = std::get<I>(m_Storages);
        return s && s->Has(entity);
    }

    bool AnyStorageNull() const {
        return AnyNull(std::index_sequence_for<Components...>{});
    }

    template<usize... Is>
    bool AnyNull(std::index_sequence<Is...>) const {
        return ((!std::get<Is>(m_Storages)) || ...);
    }
};

} // namespace ECS
} // namespace Enjin
