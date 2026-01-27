#include "Enjin/Renderer/Texture.h"
#include "Enjin/Logging/Log.h"

namespace Enjin {
namespace Renderer {

Texture::Texture(VulkanContext* context)
    : m_Context(context) {
}

Texture::~Texture() {
    Destroy();
}

bool Texture::LoadFromFile(const std::string& filepath, const SamplerConfig& samplerConfig) {
    m_Image = std::make_unique<VulkanImage>(m_Context);
    if (!m_Image->LoadFromFile(filepath)) {
        m_Image.reset();
        return false;
    }

    m_Sampler = std::make_unique<VulkanSampler>(m_Context);
    if (!m_Sampler->Create(samplerConfig)) {
        m_Image.reset();
        m_Sampler.reset();
        return false;
    }

    ENJIN_LOG_INFO(Renderer, "Loaded texture: %s (%dx%d)", filepath.c_str(), GetWidth(), GetHeight());
    return true;
}

bool Texture::CreateFromData(
    const void* data,
    u32 width,
    u32 height,
    u32 channels,
    VkFormat format,
    const SamplerConfig& samplerConfig
) {
    m_Image = std::make_unique<VulkanImage>(m_Context);
    if (!m_Image->CreateFromData(data, width, height, channels, format)) {
        m_Image.reset();
        return false;
    }

    m_Sampler = std::make_unique<VulkanSampler>(m_Context);
    if (!m_Sampler->Create(samplerConfig)) {
        m_Image.reset();
        m_Sampler.reset();
        return false;
    }

    return true;
}

bool Texture::CreateSolidColor(u8 r, u8 g, u8 b, u8 a) {
    u8 pixels[4] = { r, g, b, a };
    return CreateFromData(pixels, 1, 1, 4, VK_FORMAT_R8G8B8A8_SRGB, VulkanSampler::Nearest());
}

void Texture::Destroy() {
    m_Sampler.reset();
    m_Image.reset();
}

VkDescriptorImageInfo Texture::GetDescriptorInfo() const {
    VkDescriptorImageInfo info{};
    info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    info.imageView = GetImageView();
    info.sampler = GetSampler();
    return info;
}

} // namespace Renderer
} // namespace Enjin
