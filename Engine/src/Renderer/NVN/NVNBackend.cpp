#include "Enjin/Renderer/NVN/NVNBackend.h"

#ifdef ENJIN_PLATFORM_SWITCH

namespace Enjin {
namespace Renderer {

NVNBackend::NVNBackend() = default;

NVNBackend::~NVNBackend() {
    if (m_Initialized) {
        Shutdown();
    }
}

bool NVNBackend::Initialize(u32 width, u32 height) {
    ENJIN_LOG_WARN(Renderer, "NVN backend stub: Initialize(%u, %u) — NVN SDK not available", width, height);
    m_Width = width;
    m_Height = height;
    m_Initialized = false;
    return false;
}

void NVNBackend::Shutdown() {
    ENJIN_LOG_WARN(Renderer, "NVN backend stub: Shutdown() — NVN SDK not available");
    m_Initialized = false;
}

void NVNBackend::BeginFrame() {
    ENJIN_LOG_WARN(Renderer, "NVN backend stub: BeginFrame() — NVN SDK not available");
}

void NVNBackend::EndFrame() {
    ENJIN_LOG_WARN(Renderer, "NVN backend stub: EndFrame() — NVN SDK not available");
}

void NVNBackend::Present() {
    ENJIN_LOG_WARN(Renderer, "NVN backend stub: Present() — NVN SDK not available");
}

void NVNBackend::Resize(u32 width, u32 height) {
    ENJIN_LOG_WARN(Renderer, "NVN backend stub: Resize(%u, %u) — NVN SDK not available", width, height);
    m_Width = width;
    m_Height = height;
}

const char* NVNBackend::GetBackendName() const {
    return "NVN (Stub)";
}

PlatformCapabilities NVNBackend::GetCapabilities() const {
    PlatformCapabilities caps;
    caps.hasVulkan = false;
    caps.hasOpenGLES = false;
    caps.hasMetal = false;
    caps.hasTouchInput = true;           // Switch has touch screen in handheld
    caps.hasAccelerometer = true;        // Joy-Con accelerometer
    caps.hasGyroscope = true;            // Joy-Con gyroscope
    caps.maxTextureSize = 4096;
    caps.preferredCompression = TextureCompression::ASTC;
    return caps;
}

NVNCapabilities NVNBackend::GetNVNCapabilities() const {
    ENJIN_LOG_WARN(Renderer, "NVN backend stub: GetNVNCapabilities() — returning default Tegra X1 specs");
    return NVNCapabilities{};
}

NVNMemoryBudget NVNBackend::GetRecommendedMemoryBudget() const {
    NVNMemoryBudget budget;

    if (m_DockedMode) {
        // Docked mode: more thermal headroom, same RAM
        budget.totalAvailableBytes   = 3ULL * 1024 * 1024 * 1024;  // ~3 GB usable
        budget.gameHeapBytes         = 1500ULL * 1024 * 1024;       // 1.5 GB game heap
        budget.graphicsHeapBytes     = 1024ULL * 1024 * 1024;       // 1 GB graphics
        budget.audioHeapBytes        = 64ULL * 1024 * 1024;         // 64 MB audio
        budget.systemReservedBytes   = 460ULL * 1024 * 1024;        // ~460 MB OS/system
    } else {
        // Handheld mode: conservative to save battery + thermal
        budget.totalAvailableBytes   = 3ULL * 1024 * 1024 * 1024;
        budget.gameHeapBytes         = 1200ULL * 1024 * 1024;       // 1.2 GB game heap
        budget.graphicsHeapBytes     = 800ULL * 1024 * 1024;        // 800 MB graphics
        budget.audioHeapBytes        = 48ULL * 1024 * 1024;         // 48 MB audio
        budget.systemReservedBytes   = 460ULL * 1024 * 1024;
    }

    ENJIN_LOG_WARN(Renderer, "NVN backend stub: GetRecommendedMemoryBudget() — returning estimated budgets");
    return budget;
}

bool NVNBackend::IsDockedMode() const {
    ENJIN_LOG_WARN(Renderer, "NVN backend stub: IsDockedMode() — NVN SDK not available, returning false");
    return m_DockedMode;
}

u64 NVNBackend::CreateTexture(u32 width, u32 height, u32 format) {
    ENJIN_LOG_WARN(Renderer, "NVN backend stub: CreateTexture(%u, %u, %u) — NVN SDK not available", width, height, format);
    return 0;
}

void NVNBackend::DestroyTexture(u64 handle) {
    ENJIN_LOG_WARN(Renderer, "NVN backend stub: DestroyTexture(%llu) — NVN SDK not available",
                   static_cast<unsigned long long>(handle));
}

u64 NVNBackend::CreateVertexBuffer(u64 sizeBytes, const void* data) {
    (void)data;
    ENJIN_LOG_WARN(Renderer, "NVN backend stub: CreateVertexBuffer(%llu) — NVN SDK not available",
                   static_cast<unsigned long long>(sizeBytes));
    return 0;
}

void NVNBackend::DestroyVertexBuffer(u64 handle) {
    ENJIN_LOG_WARN(Renderer, "NVN backend stub: DestroyVertexBuffer(%llu) — NVN SDK not available",
                   static_cast<unsigned long long>(handle));
}

u64 NVNBackend::CreateShaderProgram(const void* vertexData, u64 vertexSize,
                                     const void* fragmentData, u64 fragmentSize) {
    (void)vertexData;
    (void)fragmentData;
    ENJIN_LOG_WARN(Renderer, "NVN backend stub: CreateShaderProgram(vert=%llu bytes, frag=%llu bytes) — NVN SDK not available",
                   static_cast<unsigned long long>(vertexSize),
                   static_cast<unsigned long long>(fragmentSize));
    return 0;
}

void NVNBackend::DestroyShaderProgram(u64 handle) {
    ENJIN_LOG_WARN(Renderer, "NVN backend stub: DestroyShaderProgram(%llu) — NVN SDK not available",
                   static_cast<unsigned long long>(handle));
}

void NVNBackend::Draw(u32 vertexCount, u32 instanceCount) {
    ENJIN_LOG_WARN(Renderer, "NVN backend stub: Draw(%u, %u) — NVN SDK not available", vertexCount, instanceCount);
}

void NVNBackend::DrawIndexed(u32 indexCount, u32 instanceCount) {
    ENJIN_LOG_WARN(Renderer, "NVN backend stub: DrawIndexed(%u, %u) — NVN SDK not available", indexCount, instanceCount);
}

} // namespace Renderer
} // namespace Enjin

#endif // ENJIN_PLATFORM_SWITCH
