#include "Enjin/Platform/Platform.h"

#if ENJIN_PLATFORM_WEB

// WebGPU texture creation and management is implemented inline in WebGPURenderer.cpp
// (CreateTexture, UploadTexture, DestroyTexture). This file exists as a compilation
// unit placeholder and can be extended with standalone texture utilities if needed
// (e.g., mipmap generation, compressed texture support, render target management).

#include "Enjin/Renderer/WebGPU/WebGPURenderer.h"

namespace Enjin {
namespace Renderer {

// Reserved for future texture utilities (mipmap generation, BC/ASTC decode fallbacks).

} // namespace Renderer
} // namespace Enjin

#endif // ENJIN_PLATFORM_WEB
