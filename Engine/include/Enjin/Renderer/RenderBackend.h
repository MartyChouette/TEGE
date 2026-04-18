#pragma once
#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Renderer/GPUTypes.h"
#include "Enjin/Renderer/GPUCapabilities.h"
#include "Enjin/Math/Vector.h"
#include <string>
#include <vector>
#include <memory>

namespace Enjin {
namespace Renderer {

// Forward declarations for sub-interfaces (defined in their own headers)
class IGPUBufferManager;
class IGPUTextureManager;
class IGPUPipelineManager;
class IGPUShaderManager;
class IGPUBindGroupManager;
class IRenderEncoder;

// Build target platforms
enum class BuildTarget : u8 {
    Windows,
    Linux,
    macOS,
    Android,
    iOS,
    WebGL,           // Legacy alias — use Web instead
    Web = WebGL,     // WebAssembly + WebGPU
    NintendoSwitch,  // Nintendo Switch (NVN graphics API)
    Metal            // macOS / iOS via Metal backend
};

// Texture compression format
enum class TextureCompression : u8 {
    None,
    BC1,     // DXT1 (desktop)
    BC3,     // DXT5 (desktop)
    BC7,     // High quality (desktop)
    ETC2,    // Mobile (Android/iOS)
    ASTC,    // High quality mobile
    PVRTC    // iOS legacy
};

// Platform capabilities
struct PlatformCapabilities {
    bool hasVulkan = false;
    bool hasOpenGLES = false;
    bool hasMetal = false;
    bool hasWebGPU = false;
    bool hasTouchInput = false;
    bool hasAccelerometer = false;
    bool hasGyroscope = false;
    u32 maxTextureSize = 4096;
    TextureCompression preferredCompression = TextureCompression::None;
};

// ============================================================================
// IRenderBackend — Abstract GPU backend interface
//
// Implementations: VulkanRenderer, WebGPURenderer, (future) MetalRenderer
// RenderSystem operates exclusively through this interface so it compiles
// and runs identically on all backends.
// ============================================================================
class ENJIN_API IRenderBackend {
public:
    virtual ~IRenderBackend() = default;

    // --- Lifecycle ---
    virtual bool Initialize(u32 width, u32 height) = 0;
    virtual void Shutdown() = 0;
    virtual void BeginFrame() = 0;
    virtual void EndFrame() = 0;
    virtual void Present() = 0;
    virtual void Resize(u32 width, u32 height) = 0;

    // --- Identity & capabilities ---
    virtual const char* GetBackendName() const = 0;
    virtual PlatformCapabilities GetCapabilities() const = 0;
    virtual GPUCapabilities GetGPUCapabilities() const = 0;

    // --- Frame state ---
    virtual u32 GetCurrentFrameIndex() const = 0;
    virtual u32 GetFramesInFlight() const = 0;
    virtual u32 GetSwapchainWidth() const = 0;
    virtual u32 GetSwapchainHeight() const = 0;
    virtual void WaitForAllFrames() = 0;

    // --- Resource managers ---
    // Backends create and own these; RenderSystem accesses them per-frame.
    // Returns nullptr if the manager is not yet initialized.
    virtual IGPUBufferManager*    GetBufferManager()    { return nullptr; }
    virtual IGPUTextureManager*   GetTextureManager()   { return nullptr; }
    virtual IGPUPipelineManager*  GetPipelineManager()  { return nullptr; }
    virtual IGPUShaderManager*    GetShaderManager()    { return nullptr; }
    virtual IGPUBindGroupManager* GetBindGroupManager() { return nullptr; }

    // --- Render pass management ---
    // BeginRenderPass returns an encoder for recording draw commands.
    // EndRenderPass finalizes the pass. The encoder is invalid after this.
    // For the swapchain main pass, pass a default GPURenderPassDesc (width/height=0).
    virtual IRenderEncoder* BeginRenderPass(const GPURenderPassDesc& desc) { return nullptr; }
    virtual void EndRenderPass(IRenderEncoder* encoder) {}
};

// Platform abstraction for input
struct TouchPoint {
    u32 id = 0;
    f32 x = 0.0f, y = 0.0f;
    f32 pressure = 1.0f;
    bool active = false;
};

struct AccelerometerData {
    Math::Vector3 acceleration;
    Math::Vector3 gravity;
    Math::Vector3 userAcceleration;
};

class ENJIN_API PlatformInput {
public:
    virtual ~PlatformInput() = default;

    // Touch input
    virtual u32 GetTouchCount() const { return 0; }
    virtual TouchPoint GetTouch(u32 index) const { return {}; }

    // Accelerometer
    virtual bool HasAccelerometer() const { return false; }
    virtual AccelerometerData GetAccelerometerData() const { return {}; }

    // Gyroscope
    virtual bool HasGyroscope() const { return false; }
    virtual Math::Vector3 GetGyroscopeData() const { return {}; }
};

// Build target helper
inline const char* BuildTargetToString(BuildTarget target) {
    switch (target) {
        case BuildTarget::Windows: return "Windows";
        case BuildTarget::Linux:   return "Linux";
        case BuildTarget::macOS:   return "macOS";
        case BuildTarget::Android: return "Android";
        case BuildTarget::iOS:     return "iOS";
        case BuildTarget::WebGL:   return "WebGL";
        case BuildTarget::NintendoSwitch: return "Nintendo Switch";
        case BuildTarget::Metal: return "Metal";
    }
    return "Unknown";
}

inline TextureCompression GetPreferredCompression(BuildTarget target) {
    switch (target) {
        case BuildTarget::Windows:
        case BuildTarget::Linux:
        case BuildTarget::macOS:
            return TextureCompression::BC7;
        case BuildTarget::Android:
        case BuildTarget::WebGL:
            return TextureCompression::ASTC;
        case BuildTarget::iOS:
            return TextureCompression::ASTC;
        case BuildTarget::NintendoSwitch:
            return TextureCompression::ASTC;  // Tegra X1 supports ASTC
        case BuildTarget::Metal:
            return TextureCompression::ASTC;  // Apple GPU supports ASTC natively
    }
    return TextureCompression::None;
}

} // namespace Renderer
} // namespace Enjin
