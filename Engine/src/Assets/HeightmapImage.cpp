#include "Enjin/Assets/HeightmapImage.h"
#include "Enjin/ECS/Components/Terrain.h"
#include <stb_image.h>
#include <algorithm>
#include <cmath>
#include <fstream>

namespace Enjin {
namespace Assets {

std::vector<u16> TerrainToGray16(const ECS::TerrainComponent& t) {
    const usize count = static_cast<usize>(t.gridWidth) * t.gridHeight;
    std::vector<u16> out(count, 0);
    const f32 range = t.maxHeight - t.minHeight;
    if (range <= 0.0f || t.heightmap.size() < count) return out;
    for (usize i = 0; i < count; ++i) {
        const f32 n = std::clamp((t.heightmap[i] - t.minHeight) / range, 0.0f, 1.0f);
        out[i] = static_cast<u16>(std::lround(n * 65535.0f));
    }
    return out;
}

void TerrainFromGray16(ECS::TerrainComponent& t, const u16* px, u32 w, u32 h) {
    if (!px || w == 0 || h == 0) return;
    const u32 gw = std::min(w, kHeightmapMaxGrid);
    const u32 gh = std::min(h, kHeightmapMaxGrid);
    const bool resized = gw != t.gridWidth || gh != t.gridHeight;
    t.gridWidth = gw;
    t.gridHeight = gh;
    const usize count = static_cast<usize>(gw) * gh;
    if (resized || t.splatmap.size() != count * 4) {
        t.splatmap.assign(count * 4, 0.0f);
        for (usize i = 0; i < count; ++i) t.splatmap[i * 4] = 1.0f;   // layer 0, as InitializeFlat
        t.holes.clear();
    }
    t.heightmap.assign(count, 0.0f);

    // Bilinear, sampling the image at cell centres mapped corner to corner, so
    // an image the size of the grid is copied exactly.
    auto sample = [&](u32 x, u32 y) { return static_cast<f32>(px[static_cast<usize>(y) * w + x]); };
    const f32 range = t.maxHeight - t.minHeight;
    for (u32 z = 0; z < gh; ++z) {
        const f32 fy = gh > 1 ? static_cast<f32>(z) * (h - 1) / (gh - 1) : 0.0f;
        const u32 y0 = static_cast<u32>(fy);
        const u32 y1 = std::min(y0 + 1, h - 1);
        const f32 ty = fy - y0;
        for (u32 x = 0; x < gw; ++x) {
            const f32 fx = gw > 1 ? static_cast<f32>(x) * (w - 1) / (gw - 1) : 0.0f;
            const u32 x0 = static_cast<u32>(fx);
            const u32 x1 = std::min(x0 + 1, w - 1);
            const f32 tx = fx - x0;
            const f32 top = sample(x0, y0) * (1.0f - tx) + sample(x1, y0) * tx;
            const f32 bot = sample(x0, y1) * (1.0f - tx) + sample(x1, y1) * tx;
            const f32 v = (top * (1.0f - ty) + bot * ty) / 65535.0f;
            t.heightmap[static_cast<usize>(z) * gw + x] = t.minHeight + v * range;
        }
    }
    t.meshDirty = true;
}

namespace {

u32 Crc32(const u8* data, usize len, u32 crc = 0) {
    static u32 table[256];
    static bool built = false;
    if (!built) {
        for (u32 n = 0; n < 256; ++n) {
            u32 c = n;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[n] = c;
        }
        built = true;
    }
    crc = ~crc;
    for (usize i = 0; i < len; ++i) crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

void PutU32BE(std::vector<u8>& v, u32 x) {
    v.push_back(static_cast<u8>(x >> 24));
    v.push_back(static_cast<u8>(x >> 16));
    v.push_back(static_cast<u8>(x >> 8));
    v.push_back(static_cast<u8>(x));
}

void PutChunk(std::vector<u8>& png, const char type[4], const std::vector<u8>& body) {
    PutU32BE(png, static_cast<u32>(body.size()));
    std::vector<u8> typed(type, type + 4);
    typed.insert(typed.end(), body.begin(), body.end());
    png.insert(png.end(), typed.begin(), typed.end());
    PutU32BE(png, Crc32(typed.data(), typed.size()));
}

} // namespace

std::vector<u8> EncodeGray16PNG(const u16* px, u32 w, u32 h) {
    std::vector<u8> png = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    std::vector<u8> ihdr;
    PutU32BE(ihdr, w);
    PutU32BE(ihdr, h);
    ihdr.insert(ihdr.end(), {16, 0, 0, 0, 0});   // 16-bit, greyscale, deflate, filter 0, no interlace
    PutChunk(png, "IHDR", ihdr);

    // Scanlines: filter byte 0, then big-endian samples
    std::vector<u8> raw;
    raw.reserve(static_cast<usize>(h) * (1 + static_cast<usize>(w) * 2));
    for (u32 y = 0; y < h; ++y) {
        raw.push_back(0);
        for (u32 x = 0; x < w; ++x) {
            const u16 v = px[static_cast<usize>(y) * w + x];
            raw.push_back(static_cast<u8>(v >> 8));
            raw.push_back(static_cast<u8>(v & 0xFF));
        }
    }

    // zlib stream of stored blocks
    std::vector<u8> z = {0x78, 0x01};
    usize pos = 0;
    do {
        const usize n = std::min<usize>(65535, raw.size() - pos);
        const bool last = pos + n == raw.size();
        z.push_back(last ? 1 : 0);
        z.push_back(static_cast<u8>(n & 0xFF));
        z.push_back(static_cast<u8>(n >> 8));
        z.push_back(static_cast<u8>(~n & 0xFF));
        z.push_back(static_cast<u8>((~n >> 8) & 0xFF));
        z.insert(z.end(), raw.begin() + static_cast<std::ptrdiff_t>(pos),
                 raw.begin() + static_cast<std::ptrdiff_t>(pos + n));
        pos += n;
    } while (pos < raw.size());
    u32 a = 1, b = 0;
    for (u8 c : raw) { a = (a + c) % 65521u; b = (b + a) % 65521u; }
    PutU32BE(z, (b << 16) | a);
    PutChunk(png, "IDAT", z);
    PutChunk(png, "IEND", {});
    return png;
}

bool ImportHeightmapImage(ECS::TerrainComponent& t, const std::string& path, std::string& error) {
    int w = 0, h = 0, ch = 0;
    stbi_us* px = stbi_load_16(path.c_str(), &w, &h, &ch, 1);   // one channel: luminance
    if (!px) {
        error = std::string("could not read the image: ") + (stbi_failure_reason() ? stbi_failure_reason() : "unknown");
        return false;
    }
    TerrainFromGray16(t, px, static_cast<u32>(w), static_cast<u32>(h));
    stbi_image_free(px);
    return true;
}

bool ExportHeightmapImage(const ECS::TerrainComponent& t, const std::string& path, std::string& error) {
    if (t.heightmap.size() < static_cast<usize>(t.gridWidth) * t.gridHeight) {
        error = "the terrain has no heights yet";
        return false;
    }
    const auto grey = TerrainToGray16(t);
    const auto png = EncodeGray16PNG(grey.data(), t.gridWidth, t.gridHeight);
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f.is_open() || !f.write(reinterpret_cast<const char*>(png.data()), static_cast<std::streamsize>(png.size()))) {
        error = "could not write " + path;
        return false;
    }
    return true;
}

} // namespace Assets
} // namespace Enjin
