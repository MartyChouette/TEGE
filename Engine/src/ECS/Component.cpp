#include "Enjin/ECS/Component.h"

namespace Enjin {
namespace ECS {

std::atomic<ComponentTypeId> ComponentRegistry::s_NextComponentId{1};

} // namespace ECS
} // namespace Enjin
