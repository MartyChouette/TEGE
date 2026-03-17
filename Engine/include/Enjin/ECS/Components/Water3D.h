#pragma once
#include "Enjin/Platform/Platform.h"
#include "Enjin/Effects/Water.h"

namespace Enjin { namespace ECS {

struct Water3DComponent {
    Effects::Water3DSettings settings;
    bool meshCreated = false;  // Initial mesh built by RenderSystem
};

} }
