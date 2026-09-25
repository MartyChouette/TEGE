#include "Enjin/Renderer/CaptureWrite.h"
#include "Enjin/Logging/Log.h"

#include <fstream>

// The implementation lives in the shared stb translation unit (PixelEditor.cpp
// on desktop). Declared rather than included so this file does not pull a
// 1600-line header into every build that captures a frame.
extern "C" int stbi_write_png(char const* filename, int w, int h, int comp,
                              const void* data, int stride_in_bytes);

namespace Enjin {
namespace Renderer {

bool WriteCapture(const std::string& basePath, const std::vector<u8>& rgba,
                  u32 width, u32 height) {
    const usize expected = static_cast<usize>(width) * height * 4;
    if (width == 0 || height == 0 || rgba.size() != expected) {
        ENJIN_LOG_ERROR(Renderer, "capture: refusing to write %ux%u from %zu bytes (expected %zu)",
                        width, height, rgba.size(), expected);
        return false;
    }

    // Both files are the SAME RGB. The PNG used to keep the target's alpha
    // channel, so a viewer composited it over white and a person saw a different
    // picture from the one the PPM measured and the screen showed (the screen
    // presents opaque). A capture is a picture of the frame, not of the target.
    std::vector<u8> rgb(static_cast<usize>(width) * height * 3);
    for (usize i = 0, j = 0; i < rgba.size(); i += 4, j += 3) {
        rgb[j + 0] = rgba[i + 0];
        rgb[j + 1] = rgba[i + 1];
        rgb[j + 2] = rgba[i + 2];
    }

    const std::string pngPath = basePath + ".png";
    const bool png = stbi_write_png(pngPath.c_str(), static_cast<int>(width),
                                    static_cast<int>(height), 3, rgb.data(),
                                    static_cast<int>(width * 3)) != 0;

    const std::string ppmPath = basePath + ".ppm";
    bool ppm = false;
    std::ofstream out(ppmPath, std::ios::binary);
    if (out.is_open()) {
        out << "P6\n" << width << " " << height << "\n255\n";
        out.write(reinterpret_cast<const char*>(rgb.data()),
                  static_cast<std::streamsize>(rgb.size()));
        ppm = out.good();
    }

    if (!png || !ppm) {
        ENJIN_LOG_ERROR(Renderer, "capture: write failed (png=%d ppm=%d) for %s",
                        int(png), int(ppm), basePath.c_str());
        return false;
    }
    ENJIN_LOG_INFO(Renderer, "capture: %ux%u -> %s(.png/.ppm)", width, height, basePath.c_str());
    return true;
}

} // namespace Renderer
} // namespace Enjin
