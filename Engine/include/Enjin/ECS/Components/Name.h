#pragma once

#include "Enjin/Platform/Platform.h"
#include <string>

namespace Enjin {
namespace ECS {

// Name component for identifying entities
struct NameComponent {
    std::string name;

    NameComponent() = default;
    NameComponent(const std::string& n) : name(n) {}
    NameComponent(const char* n) : name(n) {}
};

} // namespace ECS
} // namespace Enjin
