#pragma once
// WHOLE_FILE_WEBGPU_GUARD
#if !ENJIN_RENDERER_WEBGPU

#include "Enjin/Platform/Platform.h"
#include "Enjin/Renderer/VirtualTexture/VirtualTextureSystem.h"
#include <vector>

namespace Enjin {
namespace Renderer {

// Feedback buffer for virtual texturing.
// A low-resolution render pass writes page IDs (texture ID + mip + page x/y)
// to a small buffer. The CPU reads this back to determine which pages are needed.
class FeedbackBuffer {
public:
    FeedbackBuffer(u32 width, u32 height);

    // Analyze feedback data and generate page requests
    void Analyze(const void* feedbackData, std::vector<PageRequest>& outRequests);

    u32 GetWidth() const { return m_Width; }
    u32 GetHeight() const { return m_Height; }

private:
    u32 m_Width;
    u32 m_Height;
};

} // namespace Renderer
} // namespace Enjin
#endif // !ENJIN_RENDERER_WEBGPU
