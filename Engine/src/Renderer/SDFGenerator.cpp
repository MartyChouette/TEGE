#include "Enjin/Renderer/SDFGenerator.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace Enjin {
namespace Renderer {

static float DistFromOffset(int dx, int dy) {
    return std::sqrt(static_cast<float>(dx * dx + dy * dy));
}

void SDFGenerator::ComputeDistanceField(const std::vector<bool>& inside,
                                        uint32_t w, uint32_t h,
                                        std::vector<float>& outDistances) {
    // S22: Validate dimensions to prevent integer overflow
    if (w > 4096 || h > 4096) {
        outDistances.clear();
        return;
    }
    size_t sz = static_cast<size_t>(w) * static_cast<size_t>(h);

    std::vector<int> gdx(sz);
    std::vector<int> gdy(sz);
    for (uint32_t i = 0; i < sz; i++) {
        if (inside[i]) {
            gdx[i] = 0;
            gdy[i] = 0;
        } else {
            gdx[i] = static_cast<int>(w);
            gdy[i] = static_cast<int>(h);
        }
    }

    // Neighbor update helper
    auto tryUp = [&](uint32_t di, uint32_t si, int offX, int offY) {
        int cdx = gdx[si] + offX;
        int cdy = gdy[si] + offY;
        if (DistFromOffset(cdx, cdy) < DistFromOffset(gdx[di], gdy[di])) {
            gdx[di] = cdx;
            gdy[di] = cdy;
        }
    };

    // Forward pass (top-left to bottom-right)
    for (uint32_t fy = 0; fy < h; fy++) {
        for (uint32_t fx = 0; fx < w; fx++) {
            uint32_t di = fy * w + fx;
            if (fy > 0) {
                if (fx > 0)     tryUp(di, (fy - 1) * w + (fx - 1),  1,  1);
                                tryUp(di, (fy - 1) * w + fx,         0,  1);
                if (fx + 1 < w) tryUp(di, (fy - 1) * w + (fx + 1), -1,  1);
            }
            if (fx > 0)         tryUp(di, fy * w + (fx - 1),         1,  0);
        }
        // Right-to-left for (+1, 0)
        for (int rx = static_cast<int>(w) - 1; rx >= 0; rx--) {
            uint32_t ux = static_cast<uint32_t>(rx);
            if (ux + 1 < w) {
                uint32_t di = fy * w + ux;
                uint32_t si = fy * w + (ux + 1);
                tryUp(di, si, -1, 0);
            }
        }
    }

    // Backward pass (bottom-right to top-left)
    for (int by = static_cast<int>(h) - 1; by >= 0; by--) {
        uint32_t uy = static_cast<uint32_t>(by);
        for (int bx = static_cast<int>(w) - 1; bx >= 0; bx--) {
            uint32_t ux = static_cast<uint32_t>(bx);
            uint32_t di = uy * w + ux;
            if (uy + 1 < h) {
                if (ux + 1 < w) tryUp(di, (uy + 1) * w + (ux + 1), -1, -1);
                                tryUp(di, (uy + 1) * w + ux,          0, -1);
                if (ux > 0)     tryUp(di, (uy + 1) * w + (ux - 1),  1, -1);
            }
            if (ux + 1 < w)     tryUp(di, uy * w + (ux + 1),        -1,  0);
        }
        // Left-to-right for (-1, 0)
        for (uint32_t lx = 0; lx < w; lx++) {
            if (lx > 0) {
                uint32_t di = uy * w + lx;
                uint32_t si = uy * w + (lx - 1);
                tryUp(di, si, 1, 0);
            }
        }
    }

    outDistances.resize(sz);
    for (uint32_t i = 0; i < sz; i++) {
        outDistances[i] = DistFromOffset(gdx[i], gdy[i]);
    }
}

SDFGenerator::SDFResult SDFGenerator::Generate(const uint8_t* alphaPixels,
                                               uint32_t w, uint32_t h, float spread) {
    SDFResult result;
    result.width = w;
    result.height = h;

    if (!alphaPixels || w == 0 || h == 0) return result;

    // S22: Validate dimensions to prevent integer overflow
    if (w > 4096 || h > 4096) return result;

    size_t sz = static_cast<size_t>(w) * static_cast<size_t>(h);

    std::vector<bool> insideMask(sz);
    std::vector<bool> outsideMask(sz);
    for (uint32_t i = 0; i < sz; i++) {
        bool opaque = alphaPixels[i * 4 + 3] > 127;
        insideMask[i] = opaque;
        outsideMask[i] = !opaque;
    }

    std::vector<float> insideDist;
    ComputeDistanceField(insideMask, w, h, insideDist);

    std::vector<float> outsideDist;
    ComputeDistanceField(outsideMask, w, h, outsideDist);

    result.distances.resize(sz);
    for (uint32_t i = 0; i < sz; i++) {
        float d = insideMask[i] ? -outsideDist[i] : insideDist[i];
        result.distances[i] = std::clamp(d, -spread, spread);
    }

    return result;
}

std::vector<uint8_t> SDFGenerator::ToRGBA(const SDFResult& sdf) {
    size_t sz = static_cast<size_t>(sdf.width) * static_cast<size_t>(sdf.height);
    std::vector<uint8_t> rgba(static_cast<size_t>(sz) * 4, static_cast<uint8_t>(255));

    if (sdf.distances.empty()) return rgba;

    float maxAbsDist = 1.0f;
    for (uint32_t i = 0; i < sz; i++) {
        float ad = std::abs(sdf.distances[i]);
        if (ad > maxAbsDist) maxAbsDist = ad;
    }

    for (uint32_t i = 0; i < sz; i++) {
        float normalized = 0.5f - sdf.distances[i] / (2.0f * maxAbsDist);
        normalized = std::clamp(normalized, 0.0f, 1.0f);
        uint8_t alpha = static_cast<uint8_t>(normalized * 255.0f);

        rgba[i * 4 + 0] = 255;
        rgba[i * 4 + 1] = 255;
        rgba[i * 4 + 2] = 255;
        rgba[i * 4 + 3] = alpha;
    }

    return rgba;
}

} // namespace Renderer
} // namespace Enjin
