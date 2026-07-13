#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/Entity.h"
#include <atomic>
#include <typeindex>
#include <vector>

// Platform-specific prefetch intrinsic
#ifdef ENJIN_COMPILER_MSVC
    #include <intrin.h>
    #define ENJIN_PREFETCH(addr) _mm_prefetch(reinterpret_cast<const char*>(addr), _MM_HINT_T0)
#elif defined(__GNUC__) || defined(__clang__)
    #define ENJIN_PREFETCH(addr) __builtin_prefetch(addr, 0, 3)
#else
    #define ENJIN_PREFETCH(addr) ((void)0)
#endif

namespace Enjin {
namespace ECS {

// Component type ID
using ComponentTypeId = u32;

// Base component interface
struct ENJIN_API IComponent {
    virtual ~IComponent() = default;
};

// Component registry - manages component type IDs
// Uses typeid hash for cross-translation-unit consistency (fixes WASM template static duplication)
class ENJIN_API ComponentRegistry {
public:
    template<typename T>
    static ComponentTypeId GetTypeId() {
        return GetOrAssignTypeId(typeid(T).hash_code());
    }

    static ComponentTypeId GetNextId() { return s_NextComponentId.load(std::memory_order_relaxed); }

private:
    static ComponentTypeId GetOrAssignTypeId(std::size_t typeHash);
    static std::atomic<ComponentTypeId> s_NextComponentId;
};

// Sentinel value for sparse array slots that have no mapping
static constexpr usize SPARSE_EMPTY = static_cast<usize>(-1);

// Component storage — packed dense arrays with sparse-array entity-to-index mapping.
//
// Previous implementation used std::unordered_map<Entity, usize> for the entity→index
// lookup. Every Has() and Get() call paid the cost of hashing + bucket traversal.
// A flat sparse array gives true O(1) lookup with zero hash overhead — just a
// bounds check and an array dereference.
//
// GENERATIONAL HANDLES: Entity packs a generation counter in the high 32 bits
// (Entity.h), so the sparse array is indexed by EntityIndex(entity) — NEVER by
// the raw u64 handle. A recycled handle (generation >= 1) cast to an array index
// is a multi-billion-element resize (the c8471de RenderSystem crash class).
// Because a recycled slot index can map to a dense entry left behind by a dead
// predecessor, every lookup verifies ownership against the full handle stored in
// m_Entities; a generation mismatch is a miss, not the predecessor's component.
//
// Memory trade-off: the sparse array may have gaps for destroyed entities, but
// slot indices are recycled (EntityManager::m_FreeIndices), so the sparse array
// stays compact in practice. For 1000 entities, this costs ~8 KB vs the
// unordered_map's ~48+ KB (hash table overhead with load factor).
//
// The dense component and entity arrays remain packed and contiguous for cache-friendly
// iteration. Swap-and-pop removal preserves density.
template<typename T>
class ComponentStorage {
public:
    T& Add(Entity entity) {
        usize eid = EntityIndex(entity);
        EnsureSparseCapacity(eid);

        usize existing = m_Sparse[eid];
        if (existing != SPARSE_EMPTY) {
            // The slot still maps to a dense entry from a previous generation
            // of this index (destroyed entity whose component was never
            // removed). Reclaim the entry in place instead of leaking it.
            m_Entities[existing] = entity;
            m_Components[existing] = T{};
            return m_Components[existing];
        }

        usize denseIndex = m_Components.size();
        m_Entities.push_back(entity);
        m_Components.push_back(T{});
        m_Sparse[eid] = denseIndex;
        return m_Components.back();
    }

    void Remove(Entity entity) {
        usize eid = EntityIndex(entity);
        if (eid >= m_Sparse.size() || m_Sparse[eid] == SPARSE_EMPTY) {
            return;
        }

        usize index = m_Sparse[eid];
        if (m_Entities[index] != entity) {
            return;  // stale handle from a previous generation of this slot
        }
        usize lastIndex = m_Components.size() - 1;

        // Swap with last element to keep arrays dense
        if (index != lastIndex) {
            m_Components[index] = std::move(m_Components[lastIndex]);
            m_Entities[index] = m_Entities[lastIndex];
            m_Sparse[EntityIndex(m_Entities[index])] = index;
        }

        m_Components.pop_back();
        m_Entities.pop_back();
        m_Sparse[eid] = SPARSE_EMPTY;
    }

    ENJIN_FORCE_INLINE T* Get(Entity entity) {
        usize eid = EntityIndex(entity);
        if (eid >= m_Sparse.size()) return nullptr;
        usize idx = m_Sparse[eid];
        if (idx == SPARSE_EMPTY || m_Entities[idx] != entity) return nullptr;
        return &m_Components[idx];
    }

    ENJIN_FORCE_INLINE const T* Get(Entity entity) const {
        usize eid = EntityIndex(entity);
        if (eid >= m_Sparse.size()) return nullptr;
        usize idx = m_Sparse[eid];
        if (idx == SPARSE_EMPTY || m_Entities[idx] != entity) return nullptr;
        return &m_Components[idx];
    }

    ENJIN_FORCE_INLINE bool Has(Entity entity) const {
        usize eid = EntityIndex(entity);
        if (eid >= m_Sparse.size()) return false;
        usize idx = m_Sparse[eid];
        return idx != SPARSE_EMPTY && m_Entities[idx] == entity;
    }

    // Get component by dense index (no entity lookup). Use when iterating
    // the dense arrays directly (e.g., in ForEach or bulk operations).
    ENJIN_FORCE_INLINE T& GetByIndex(usize denseIndex) {
        return m_Components[denseIndex];
    }
    ENJIN_FORCE_INLINE const T& GetByIndex(usize denseIndex) const {
        return m_Components[denseIndex];
    }

    // Get the dense index for an entity (or SPARSE_EMPTY if not present).
    // Useful for prefetch calculations.
    ENJIN_FORCE_INLINE usize GetDenseIndex(Entity entity) const {
        usize eid = EntityIndex(entity);
        if (eid >= m_Sparse.size()) return SPARSE_EMPTY;
        usize idx = m_Sparse[eid];
        if (idx != SPARSE_EMPTY && m_Entities[idx] != entity) return SPARSE_EMPTY;
        return idx;
    }

    // Prefetch a component into L1 cache by entity ID.
    // Call this 2-4 iterations ahead to hide memory latency.
    ENJIN_FORCE_INLINE void Prefetch(Entity entity) const {
        usize eid = EntityIndex(entity);
        if (eid < m_Sparse.size()) {
            usize idx = m_Sparse[eid];
            if (idx != SPARSE_EMPTY && idx < m_Components.size()) {
                ENJIN_PREFETCH(&m_Components[idx]);
            }
        }
    }

    void Clear() {
        m_Components.clear();
        m_Entities.clear();
        m_Sparse.clear();
    }

    usize Size() const { return m_Components.size(); }
    bool Empty() const { return m_Components.empty(); }

    // Iteration support
    const std::vector<Entity>& GetEntities() const { return m_Entities; }
    const std::vector<T>& GetComponents() const { return m_Components; }
    std::vector<T>& GetComponents() { return m_Components; }

    // Raw data pointer for bulk operations (memcpy to GPU, etc.)
    T* Data() { return m_Components.data(); }
    const T* Data() const { return m_Components.data(); }

private:
    // Takes the SLOT INDEX (EntityIndex), never the raw generational handle.
    void EnsureSparseCapacity(usize eid) {
        if (eid >= m_Sparse.size()) {
            // Grow with some headroom to avoid frequent reallocations
            usize newSize = eid + 1;
            if (newSize < m_Sparse.size() * 2) newSize = m_Sparse.size() * 2;
            if (newSize < 64) newSize = 64;
            m_Sparse.resize(newSize, SPARSE_EMPTY);
        }
    }

    std::vector<Entity> m_Entities;      // Dense: entity IDs (packed, no gaps)
    std::vector<T>      m_Components;    // Dense: component data (packed, no gaps)
    std::vector<usize>  m_Sparse;        // Sparse: entity ID → dense index (or SPARSE_EMPTY)
};

} // namespace ECS
} // namespace Enjin
