#include "Enjin/Platform/Platform.h"

#if ENJIN_PLATFORM_WEB

// WebGPU buffer creation and management is implemented inline in WebGPURenderer.cpp
// (CreateBuffer, UpdateBuffer, DestroyBuffer). This file exists as a compilation
// unit placeholder and can be extended with standalone buffer utilities if needed
// (e.g., staging ring buffers, dynamic uniform buffer sub-allocation).

#include "Enjin/Renderer/WebGPU/WebGPURenderer.h"

namespace Enjin {
namespace Renderer {

// Reserved for future buffer pooling / staging utilities.

} // namespace Renderer
} // namespace Enjin

#endif // ENJIN_PLATFORM_WEB
