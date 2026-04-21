#include "Enjin/ECS/Component.h"
#include <unordered_map>
#include <mutex>

namespace Enjin {
namespace ECS {

std::atomic<ComponentTypeId> ComponentRegistry::s_NextComponentId{1};

ComponentTypeId ComponentRegistry::GetOrAssignTypeId(std::size_t typeHash) {
    static std::unordered_map<std::size_t, ComponentTypeId> s_TypeMap;
    static std::mutex s_Mutex;

    std::lock_guard<std::mutex> lock(s_Mutex);
    auto it = s_TypeMap.find(typeHash);
    if (it != s_TypeMap.end()) return it->second;

    ComponentTypeId id = s_NextComponentId.fetch_add(1, std::memory_order_relaxed);
    s_TypeMap[typeHash] = id;
    return id;
}

} // namespace ECS
} // namespace Enjin
