#include "Enjin/Effects/SpriteTextureAtlas.h"
#include "Enjin/Logging/Log.h"
#include <vector>
#include <algorithm>
#include <cstring>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "stb_image.h"

namespace Enjin {
namespace Effects {

SpriteTextureAtlas::~SpriteTextureAtlas() {
    Shutdown();
}

bool SpriteTextureAtlas::Initialize(Renderer::VulkanContext* context) {
    if (m_Initialized) return true;
    if (!context) return false;

    m_Context = context;
    m_Dirty = true;
    m_Initialized = true;
    return true;
}

void SpriteTextureAtlas::Shutdown() {
    if (!m_Initialized) return;

    m_AtlasTexture.reset();
    m_Regions.clear();
    m_RequestedPaths.clear();
    m_ExcludedPaths.clear();
    m_Dirty = true;
    m_Initialized = false;
    m_Context = nullptr;
}

bool SpriteTextureAtlas::RequestTexture(const std::string& path) {
    if (path.empty() || !m_Initialized) return false;

    // Already excluded (too large or failed)
    if (m_ExcludedPaths.count(path)) return false;

    // New path — mark dirty so Build() runs
    if (m_RequestedPaths.insert(path).second) {
        m_Dirty = true;
        return true;
    }
    return false;  // Already requested
}

bool SpriteTextureAtlas::Build() {
    if (!m_Initialized || !m_Context) return false;
    if (m_RequestedPaths.empty()) {
        m_Dirty = false;
        return true;
    }

    // 1. Load all requested textures from disk
    struct LoadedTex {
        std::string path;
        stbi_uc* pixels;
        int width, height;
    };
    std::vector<LoadedTex> loaded;

    for (const auto& path : m_RequestedPaths) {
        if (m_ExcludedPaths.count(path)) continue;

        int w = 0, h = 0, ch = 0;
        stbi_uc* pixels = nullptr;

#ifdef _WIN32
        {
            int wlen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
            if (wlen > 0) {
                std::wstring wpath(wlen - 1, L'\0');
                MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, wpath.data(), wlen);
                FILE* f = _wfopen(wpath.c_str(), L"rb");
                if (f) {
                    pixels = stbi_load_from_file(f, &w, &h, &ch, STBI_rgb_alpha);
                    fclose(f);
                }
            }
        }
#else
        pixels = stbi_load(path.c_str(), &w, &h, &ch, STBI_rgb_alpha);
#endif

        if (!pixels) {
            ENJIN_LOG_WARN(Renderer, "SpriteTextureAtlas: Failed to load '%s', excluding", path.c_str());
            m_ExcludedPaths.insert(path);
            continue;
        }

        if (static_cast<u32>(w) > MAX_SUBTEXTURE_SIZE || static_cast<u32>(h) > MAX_SUBTEXTURE_SIZE) {
            stbi_image_free(pixels);
            m_ExcludedPaths.insert(path);
            continue;
        }

        loaded.push_back({ path, pixels, w, h });
    }

    if (loaded.empty()) {
        m_Dirty = false;
        return true;
    }

    // 2. Sort by height descending for efficient shelf packing
    std::sort(loaded.begin(), loaded.end(), [](const LoadedTex& a, const LoadedTex& b) {
        return a.height > b.height;
    });

    // 3. Allocate CPU atlas buffer (zeroed = transparent black)
    const u32 atlasSize = ATLAS_SIZE;
    std::vector<u8> atlasPixels(static_cast<usize>(atlasSize) * atlasSize * 4, 0);

    // 4. Shelf-pack
    m_Regions.clear();
    u32 shelfY = 0;
    u32 shelfHeight = 0;
    u32 cursorX = 0;

    for (auto& tex : loaded) {
        u32 tw = static_cast<u32>(tex.width);
        u32 th = static_cast<u32>(tex.height);
        u32 paddedW = tw + PADDING;
        u32 paddedH = th + PADDING;

        // Does it fit on the current shelf?
        if (cursorX + paddedW > atlasSize) {
            // Start new shelf
            shelfY += shelfHeight + PADDING;
            shelfHeight = 0;
            cursorX = 0;
        }

        // Does it fit vertically?
        if (shelfY + paddedH > atlasSize) {
            // Atlas is full, exclude remaining
            stbi_image_free(tex.pixels);
            tex.pixels = nullptr;
            m_ExcludedPaths.insert(tex.path);
            continue;
        }

        // Place it
        u32 px = cursorX;
        u32 py = shelfY;

        // Copy pixels row-by-row into atlas buffer
        for (u32 row = 0; row < th; ++row) {
            const u8* src = tex.pixels + row * tw * 4;
            u8* dst = atlasPixels.data() + ((py + row) * atlasSize + px) * 4;
            std::memcpy(dst, src, tw * 4);
        }

        // Record region with normalized UVs
        AtlasRegion region;
        region.x = px;
        region.y = py;
        region.width = tw;
        region.height = th;
        region.uvLeft   = static_cast<f32>(px) / static_cast<f32>(atlasSize);
        region.uvTop    = static_cast<f32>(py) / static_cast<f32>(atlasSize);
        region.uvRight  = static_cast<f32>(px + tw) / static_cast<f32>(atlasSize);
        region.uvBottom = static_cast<f32>(py + th) / static_cast<f32>(atlasSize);
        m_Regions[tex.path] = region;

        cursorX += paddedW;
        if (th > shelfHeight) shelfHeight = th;

        stbi_image_free(tex.pixels);
        tex.pixels = nullptr;
    }

    // 5. Upload to GPU
    Renderer::SamplerConfig samplerConfig;
    samplerConfig.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerConfig.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerConfig.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerConfig.maxLod = 0.0f;  // No mipmaps for atlas

    m_AtlasTexture = std::make_shared<Renderer::Texture>(m_Context);
    if (!m_AtlasTexture->CreateFromData(atlasPixels.data(), atlasSize, atlasSize, 4,
                                         VK_FORMAT_R8G8B8A8_SRGB, samplerConfig)) {
        ENJIN_LOG_ERROR(Renderer, "SpriteTextureAtlas: Failed to create GPU texture");
        m_AtlasTexture.reset();
        m_Regions.clear();
        m_Dirty = false;
        return false;
    }

    ENJIN_LOG_INFO(Renderer, "SpriteTextureAtlas: Built atlas with %zu textures (%zu excluded)",
        m_Regions.size(), m_ExcludedPaths.size());

    m_Dirty = false;
    return true;
}

const AtlasRegion* SpriteTextureAtlas::GetRegion(const std::string& path) const {
    auto it = m_Regions.find(path);
    if (it != m_Regions.end()) return &it->second;
    return nullptr;
}

void SpriteTextureAtlas::Invalidate() {
    m_Dirty = true;
    m_Regions.clear();
    m_ExcludedPaths.clear();
    m_AtlasTexture.reset();
}

void SpriteTextureAtlas::Clear() {
    m_AtlasTexture.reset();
    m_Regions.clear();
    m_RequestedPaths.clear();
    m_ExcludedPaths.clear();
    m_Dirty = true;
}

} // namespace Effects
} // namespace Enjin
