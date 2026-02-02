#pragma once
#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include <string>
#include <vector>
#include <memory>

namespace Enjin {
namespace Renderer {

// Build target platforms
enum class BuildTarget : u8 {
    Windows,
    Linux,
    macOS,
    Android,
    iOS,
    WebGL
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
    bool hasTouchInput = false;
    bool hasAccelerometer = false;
    bool hasGyroscope = false;
    u32 maxTextureSize = 4096;
    TextureCompression preferredCompression = TextureCompression::None;
};

// Abstract render backend — Vulkan implements this, future backends (Metal, GLES) can too
class ENJIN_API IRenderBackend {
public:
    virtual ~IRenderBackend() = default;

    virtual bool Initialize(u32 width, u32 height) = 0;
    virtual void Shutdown() = 0;
    virtual void BeginFrame() = 0;
    virtual void EndFrame() = 0;
    virtual void Present() = 0;
    virtual void Resize(u32 width, u32 height) = 0;

    virtual const char* GetBackendName() const = 0;
    virtual PlatformCapabilities GetCapabilities() const = 0;
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
    }
    return TextureCompression::None;
}

} // namespace Renderer
} // namespace Enjin
