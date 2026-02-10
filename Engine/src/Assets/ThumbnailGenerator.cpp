#include "Enjin/Assets/ThumbnailGenerator.h"
#include "Enjin/Logging/Log.h"
#include <algorithm>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <filesystem>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

// stb_image for thumbnail loading (implementation defined elsewhere)
#include "stb_image.h"

namespace Enjin {
namespace Assets {

const ThumbnailData ThumbnailGenerator::s_InvalidThumbnail = {};

// --- Minimal 5x7 bitmap font for labels ---
// Each character is 5 columns x 7 rows, stored as 7 bytes (each byte = one row, 5 LSBs used)
static const u8 s_Font5x7[][7] = {
    // Space (0x20 - index 0)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    // A-Z (indices 1-26)
    {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}, // A
    {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E}, // B
    {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}, // C
    {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E}, // D
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}, // E
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10}, // F
    {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F}, // G
    {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}, // H
    {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E}, // I
    {0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C}, // J
    {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}, // K
    {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}, // L
    {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11}, // M
    {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11}, // N
    {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, // O
    {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}, // P
    {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D}, // Q
    {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11}, // R
    {0x0E, 0x11, 0x10, 0x0E, 0x01, 0x11, 0x0E}, // S
    {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}, // T
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, // U
    {0x11, 0x11, 0x11, 0x11, 0x0A, 0x0A, 0x04}, // V
    {0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11}, // W
    {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11}, // X
    {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04}, // Y
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F}, // Z
    // 0-9 (indices 27-36)
    {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}, // 0
    {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E}, // 1
    {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}, // 2
    {0x0E, 0x11, 0x01, 0x06, 0x01, 0x11, 0x0E}, // 3
    {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}, // 4
    {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E}, // 5
    {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E}, // 6
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}, // 7
    {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}, // 8
    {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C}, // 9
};

static i32 GetFontIndex(char c) {
    if (c == ' ') return 0;
    if (c >= 'A' && c <= 'Z') return c - 'A' + 1;
    if (c >= 'a' && c <= 'z') return c - 'a' + 1;
    if (c >= '0' && c <= '9') return c - '0' + 27;
    return 0; // default to space
}

// --- File type helpers ---

bool ThumbnailGenerator::IsImage(const std::string& ext) {
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp" || ext == ".svg";
}

bool ThumbnailGenerator::IsModel(const std::string& ext) {
    return ext == ".gltf" || ext == ".glb" || ext == ".fbx" || ext == ".obj" ||
           ext == ".dae" || ext == ".3ds" || ext == ".ply" || ext == ".vox";
}

bool ThumbnailGenerator::IsScene(const std::string& ext) {
    return ext == ".enjin" || ext == ".json";
}

bool ThumbnailGenerator::IsAudio(const std::string& ext) {
    return ext == ".wav" || ext == ".mp3" || ext == ".ogg" || ext == ".flac";
}

bool ThumbnailGenerator::IsScript(const std::string& ext) {
    return ext == ".as" || ext == ".angelscript";
}

// --- Drawing primitives ---

void ThumbnailGenerator::DrawLine(std::vector<u8>& pixels, u32 w, u32 h,
                                   i32 x0, i32 y0, i32 x1, i32 y1, u32 color) {
    u8 r = (color >> 24) & 0xFF;
    u8 g = (color >> 16) & 0xFF;
    u8 b = (color >> 8) & 0xFF;
    u8 a = color & 0xFF;

    i32 dx = std::abs(x1 - x0), dy = std::abs(y1 - y0);
    i32 sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    i32 err = dx - dy;

    for (;;) {
        if (x0 >= 0 && x0 < static_cast<i32>(w) && y0 >= 0 && y0 < static_cast<i32>(h)) {
            u32 idx = (y0 * w + x0) * 4;
            pixels[idx]     = r;
            pixels[idx + 1] = g;
            pixels[idx + 2] = b;
            pixels[idx + 3] = a;
        }
        if (x0 == x1 && y0 == y1) break;
        i32 e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx) { err += dx; y0 += sy; }
    }
}

void ThumbnailGenerator::DrawTriangle(std::vector<u8>& pixels, std::vector<f32>& depthBuf,
                                       u32 w, u32 h,
                                       f32 x0, f32 y0, f32 z0,
                                       f32 x1, f32 y1, f32 z1,
                                       f32 x2, f32 y2, f32 z2,
                                       u32 color) {
    u8 r = (color >> 24) & 0xFF;
    u8 g = (color >> 16) & 0xFF;
    u8 b = (color >> 8) & 0xFF;
    u8 a = color & 0xFF;

    // Bounding box
    i32 minX = std::max(0, static_cast<i32>(std::floor(std::min({x0, x1, x2}))));
    i32 maxX = std::min(static_cast<i32>(w) - 1, static_cast<i32>(std::ceil(std::max({x0, x1, x2}))));
    i32 minY = std::max(0, static_cast<i32>(std::floor(std::min({y0, y1, y2}))));
    i32 maxY = std::min(static_cast<i32>(h) - 1, static_cast<i32>(std::ceil(std::max({y0, y1, y2}))));

    f32 denom = (y1 - y2) * (x0 - x2) + (x2 - x1) * (y0 - y2);
    if (std::abs(denom) < 1e-6f) return;

    for (i32 py = minY; py <= maxY; ++py) {
        for (i32 px = minX; px <= maxX; ++px) {
            f32 fx = px + 0.5f, fy = py + 0.5f;
            f32 w0 = ((y1 - y2) * (fx - x2) + (x2 - x1) * (fy - y2)) / denom;
            f32 w1 = ((y2 - y0) * (fx - x2) + (x0 - x2) * (fy - y2)) / denom;
            f32 w2 = 1.0f - w0 - w1;

            if (w0 >= 0 && w1 >= 0 && w2 >= 0) {
                f32 z = w0 * z0 + w1 * z1 + w2 * z2;
                u32 idx = py * w + px;
                if (z < depthBuf[idx]) {
                    depthBuf[idx] = z;
                    u32 pidx = idx * 4;
                    pixels[pidx]     = r;
                    pixels[pidx + 1] = g;
                    pixels[pidx + 2] = b;
                    pixels[pidx + 3] = a;
                }
            }
        }
    }
}

void ThumbnailGenerator::DrawLabel(std::vector<u8>& pixels, u32 w, u32 h,
                                    const char* text, i32 x, i32 y, u32 color) {
    u8 r = (color >> 24) & 0xFF;
    u8 g = (color >> 16) & 0xFF;
    u8 b = (color >> 8) & 0xFF;
    u8 a = color & 0xFF;

    i32 cx = x;
    while (*text) {
        i32 fi = GetFontIndex(*text);
        const u8* glyph = s_Font5x7[fi];
        for (i32 row = 0; row < 7; ++row) {
            for (i32 col = 0; col < 5; ++col) {
                if (glyph[row] & (1 << (4 - col))) {
                    i32 px = cx + col;
                    i32 py = y + row;
                    if (px >= 0 && px < static_cast<i32>(w) && py >= 0 && py < static_cast<i32>(h)) {
                        u32 idx = (py * w + px) * 4;
                        pixels[idx]     = r;
                        pixels[idx + 1] = g;
                        pixels[idx + 2] = b;
                        pixels[idx + 3] = a;
                    }
                }
            }
        }
        cx += 6; // 5 wide + 1 spacing
        ++text;
    }
}

// --- Projection for model thumbnails ---

void ThumbnailGenerator::ProjectVertex(const SoftVertex& v, f32 scale, f32 cx, f32 cy,
                                        f32 rotY, f32 rotX, f32& outX, f32& outY, f32& outDepth) const {
    // Rotate around Y axis
    f32 cosY = std::cos(rotY), sinY = std::sin(rotY);
    f32 rx = v.x * cosY + v.z * sinY;
    f32 ry = v.y;
    f32 rz = -v.x * sinY + v.z * cosY;

    // Rotate around X axis
    f32 cosX = std::cos(rotX), sinX = std::sin(rotX);
    f32 fy = ry * cosX - rz * sinX;
    f32 fz = ry * sinX + rz * cosX;

    outX = cx + rx * scale;
    outY = cy - fy * scale;
    outDepth = fz;
}

// --- Thumbnail generation ---

ThumbnailData ThumbnailGenerator::Generate(const std::string& path, ThumbnailSize size) {
    // Check cache
    auto it = m_Cache.find(path);
    if (it != m_Cache.end()) return it->second;

    u32 targetSize = static_cast<u32>(size);

    std::filesystem::path p(path);
    std::string ext = p.extension().string();
    for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    ThumbnailData result;

    if (IsImage(ext)) {
        result = GenerateImageThumbnail(path, targetSize);
    } else if (IsModel(ext)) {
        result = GenerateModelThumbnail(path, targetSize);
    } else if (IsScene(ext)) {
        result = GenerateSceneThumbnail(path, targetSize);
    } else if (IsAudio(ext)) {
        result = GeneratePlaceholder("SFX", 0x50FFCC80, targetSize);
    } else if (IsScript(ext)) {
        result = GeneratePlaceholder("AS", 0x60AAFF80, targetSize);
    } else {
        result = GeneratePlaceholder("FILE", 0xAAAAAAFF, targetSize);
    }

    // Cache the result
    if (result.valid) {
        m_Cache[path] = result;
    }
    return result;
}

ThumbnailData ThumbnailGenerator::GenerateImageThumbnail(const std::string& path, u32 targetSize) {
    ThumbnailData result;

    int w, h, ch;
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

    if (!pixels) return result;

    // Downsample to target size (box filter)
    f32 scaleX = static_cast<f32>(w) / targetSize;
    f32 scaleY = static_cast<f32>(h) / targetSize;
    f32 scale = std::max(scaleX, scaleY);
    if (scale < 1.0f) scale = 1.0f;

    u32 outW = std::max(1u, static_cast<u32>(w / scale));
    u32 outH = std::max(1u, static_cast<u32>(h / scale));

    result.width = outW;
    result.height = outH;
    result.pixels.resize(outW * outH * 4);

    for (u32 y = 0; y < outH; ++y) {
        for (u32 x = 0; x < outW; ++x) {
            // Box filter sample area
            f32 sx0 = x * scale;
            f32 sy0 = y * scale;
            f32 sx1 = std::min(sx0 + scale, static_cast<f32>(w));
            f32 sy1 = std::min(sy0 + scale, static_cast<f32>(h));

            u32 r = 0, g = 0, b = 0, a = 0, count = 0;
            for (u32 sy = static_cast<u32>(sy0); sy < static_cast<u32>(sy1) && sy < static_cast<u32>(h); ++sy) {
                for (u32 sx = static_cast<u32>(sx0); sx < static_cast<u32>(sx1) && sx < static_cast<u32>(w); ++sx) {
                    const u8* p = pixels + (sy * w + sx) * 4;
                    r += p[0]; g += p[1]; b += p[2]; a += p[3];
                    ++count;
                }
            }
            if (count > 0) {
                u32 idx = (y * outW + x) * 4;
                result.pixels[idx]     = static_cast<u8>(r / count);
                result.pixels[idx + 1] = static_cast<u8>(g / count);
                result.pixels[idx + 2] = static_cast<u8>(b / count);
                result.pixels[idx + 3] = static_cast<u8>(a / count);
            }
        }
    }

    stbi_image_free(pixels);
    result.valid = true;
    return result;
}

ThumbnailData ThumbnailGenerator::GenerateModelThumbnail(const std::string& path, u32 targetSize) {
    ThumbnailData result;
    result.width = targetSize;
    result.height = targetSize;
    result.pixels.resize(targetSize * targetSize * 4, 0);
    std::vector<f32> depthBuf(targetSize * targetSize, 1e30f);

    // Dark background
    for (u32 i = 0; i < targetSize * targetSize; ++i) {
        result.pixels[i * 4]     = 30;
        result.pixels[i * 4 + 1] = 32;
        result.pixels[i * 4 + 2] = 38;
        result.pixels[i * 4 + 3] = 255;
    }

    // Try to load the model and extract basic geometry
    // For a lightweight thumbnail, we parse OBJ/glTF vertex positions directly
    // without pulling in the full asset import pipeline
    std::vector<SoftVertex> vertices;
    std::vector<u32> indices;

    std::filesystem::path p(path);
    std::string ext = p.extension().string();
    for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    if (ext == ".obj") {
        // Quick OBJ parser for vertex positions only
        FILE* f = nullptr;
#ifdef _WIN32
        {
            int wlen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
            if (wlen > 0) {
                std::wstring wpath(wlen - 1, L'\0');
                MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, wpath.data(), wlen);
                f = _wfopen(wpath.c_str(), L"r");
            }
        }
#else
        f = fopen(path.c_str(), "r");
#endif
        if (f) {
            char line[512];
            std::vector<SoftVertex> positions;
            while (fgets(line, sizeof(line), f)) {
                if (line[0] == 'v' && line[1] == ' ') {
                    SoftVertex sv = {};
                    sscanf(line + 2, "%f %f %f", &sv.x, &sv.y, &sv.z);
                    positions.push_back(sv);
                } else if (line[0] == 'f' && line[1] == ' ') {
                    // Parse face indices (v or v/vt/vn format)
                    u32 faceIdx[16];
                    u32 faceCount = 0;
                    const char* ptr = line + 2;
                    while (*ptr && faceCount < 16) {
                        while (*ptr == ' ') ++ptr;
                        if (!*ptr || *ptr == '\n') break;
                        i32 vi = atoi(ptr) - 1; // OBJ is 1-indexed
                        if (vi >= 0 && vi < static_cast<i32>(positions.size())) {
                            faceIdx[faceCount++] = static_cast<u32>(vi);
                        }
                        // Skip to next space or end
                        while (*ptr && *ptr != ' ' && *ptr != '\n') ++ptr;
                    }
                    // Triangulate fan
                    for (u32 i = 2; i < faceCount; ++i) {
                        indices.push_back(faceIdx[0]);
                        indices.push_back(faceIdx[i - 1]);
                        indices.push_back(faceIdx[i]);
                    }
                }
            }
            fclose(f);
            vertices = std::move(positions);
        }
    }

    if (vertices.empty()) {
        // Fallback: draw a 3D cube wireframe icon
        SoftVertex cubeVerts[8] = {
            {-1, -1, -1, 0, 0, 0}, { 1, -1, -1, 0, 0, 0},
            { 1,  1, -1, 0, 0, 0}, {-1,  1, -1, 0, 0, 0},
            {-1, -1,  1, 0, 0, 0}, { 1, -1,  1, 0, 0, 0},
            { 1,  1,  1, 0, 0, 0}, {-1,  1,  1, 0, 0, 0},
        };
        u32 cubeEdges[][2] = {
            {0,1},{1,2},{2,3},{3,0}, {4,5},{5,6},{6,7},{7,4},
            {0,4},{1,5},{2,6},{3,7}
        };

        f32 s = targetSize * 0.25f;
        f32 cx = targetSize * 0.5f, cy = targetSize * 0.5f;
        f32 rotY = 0.6f, rotX = 0.4f;

        for (auto& edge : cubeEdges) {
            f32 x0, y0, z0, x1, y1, z1;
            ProjectVertex(cubeVerts[edge[0]], s, cx, cy, rotY, rotX, x0, y0, z0);
            ProjectVertex(cubeVerts[edge[1]], s, cx, cy, rotY, rotX, x1, y1, z1);
            DrawLine(result.pixels, targetSize, targetSize,
                     static_cast<i32>(x0), static_cast<i32>(y0),
                     static_cast<i32>(x1), static_cast<i32>(y1),
                     0x66CCFFFF);
        }

        // Type label
        DrawLabel(result.pixels, targetSize, targetSize, "3D",
                  static_cast<i32>(targetSize * 0.5f - 6), static_cast<i32>(targetSize - 12), 0xAADDFFFF);
    } else {
        // Compute bounding box
        f32 minX = 1e30f, minY = 1e30f, minZ = 1e30f;
        f32 maxX = -1e30f, maxY = -1e30f, maxZ = -1e30f;
        for (const auto& v : vertices) {
            if (v.x < minX) minX = v.x; if (v.x > maxX) maxX = v.x;
            if (v.y < minY) minY = v.y; if (v.y > maxY) maxY = v.y;
            if (v.z < minZ) minZ = v.z; if (v.z > maxZ) maxZ = v.z;
        }

        // Center and scale to fit
        f32 centerX = (minX + maxX) * 0.5f;
        f32 centerY = (minY + maxY) * 0.5f;
        f32 centerZ = (minZ + maxZ) * 0.5f;
        f32 extent = std::max({maxX - minX, maxY - minY, maxZ - minZ});
        if (extent < 1e-6f) extent = 1.0f;

        // Normalize vertices to [-1, 1]
        for (auto& v : vertices) {
            v.x = (v.x - centerX) / (extent * 0.5f);
            v.y = (v.y - centerY) / (extent * 0.5f);
            v.z = (v.z - centerZ) / (extent * 0.5f);
        }

        f32 scale = targetSize * 0.35f;
        f32 cx = targetSize * 0.5f, cy = targetSize * 0.5f;
        f32 rotY = 0.6f, rotX = 0.4f; // isometric-ish angle

        if (!indices.empty()) {
            // Flat-shaded triangles
            f32 lightDir[3] = {0.4f, 0.7f, 0.5f};
            f32 lightLen = std::sqrt(lightDir[0]*lightDir[0] + lightDir[1]*lightDir[1] + lightDir[2]*lightDir[2]);
            lightDir[0] /= lightLen; lightDir[1] /= lightLen; lightDir[2] /= lightLen;

            for (usize i = 0; i + 2 < indices.size(); i += 3) {
                if (indices[i] >= vertices.size() || indices[i+1] >= vertices.size() || indices[i+2] >= vertices.size())
                    continue;

                const auto& v0 = vertices[indices[i]];
                const auto& v1 = vertices[indices[i+1]];
                const auto& v2 = vertices[indices[i+2]];

                // Face normal
                f32 e1x = v1.x - v0.x, e1y = v1.y - v0.y, e1z = v1.z - v0.z;
                f32 e2x = v2.x - v0.x, e2y = v2.y - v0.y, e2z = v2.z - v0.z;
                f32 nx = e1y * e2z - e1z * e2y;
                f32 ny = e1z * e2x - e1x * e2z;
                f32 nz = e1x * e2y - e1y * e2x;
                f32 nLen = std::sqrt(nx*nx + ny*ny + nz*nz);
                if (nLen > 1e-6f) { nx /= nLen; ny /= nLen; nz /= nLen; }

                f32 ndotl = std::max(0.0f, nx * lightDir[0] + ny * lightDir[1] + nz * lightDir[2]);
                f32 brightness = 0.2f + 0.8f * ndotl;

                u8 cr = static_cast<u8>(std::min(255.0f, 100 * brightness + 80));
                u8 cg = static_cast<u8>(std::min(255.0f, 160 * brightness + 60));
                u8 cb = static_cast<u8>(std::min(255.0f, 200 * brightness + 55));
                u32 triColor = (static_cast<u32>(cr) << 24) | (static_cast<u32>(cg) << 16) |
                               (static_cast<u32>(cb) << 8) | 0xFF;

                f32 px0, py0, pz0, px1, py1, pz1, px2, py2, pz2;
                ProjectVertex(v0, scale, cx, cy, rotY, rotX, px0, py0, pz0);
                ProjectVertex(v1, scale, cx, cy, rotY, rotX, px1, py1, pz1);
                ProjectVertex(v2, scale, cx, cy, rotY, rotX, px2, py2, pz2);

                DrawTriangle(result.pixels, depthBuf, targetSize, targetSize,
                             px0, py0, pz0, px1, py1, pz1, px2, py2, pz2, triColor);
            }
        } else {
            // Point cloud: draw each vertex as a dot
            for (const auto& v : vertices) {
                f32 px, py, pz;
                ProjectVertex(v, scale, cx, cy, rotY, rotX, px, py, pz);
                i32 ix = static_cast<i32>(px), iy = static_cast<i32>(py);
                if (ix >= 0 && ix < static_cast<i32>(targetSize) && iy >= 0 && iy < static_cast<i32>(targetSize)) {
                    u32 idx = (iy * targetSize + ix) * 4;
                    result.pixels[idx] = 100; result.pixels[idx+1] = 200;
                    result.pixels[idx+2] = 255; result.pixels[idx+3] = 255;
                }
            }
        }

        DrawLabel(result.pixels, targetSize, targetSize, "3D",
                  static_cast<i32>(targetSize * 0.5f - 6), static_cast<i32>(targetSize - 12), 0xAADDFFFF);
    }

    result.valid = true;
    return result;
}

ThumbnailData ThumbnailGenerator::GenerateSceneThumbnail(const std::string& /*path*/, u32 targetSize) {
    return GeneratePlaceholder("SCN", 0x66FF66FF, targetSize);
}

ThumbnailData ThumbnailGenerator::GeneratePlaceholder(const char* label, u32 color, u32 targetSize) {
    ThumbnailData result;
    result.width = targetSize;
    result.height = targetSize;
    result.pixels.resize(targetSize * targetSize * 4);

    u8 bgR = 35, bgG = 35, bgB = 40;
    u8 borderR = (color >> 24) & 0xFF;
    u8 borderG = (color >> 16) & 0xFF;
    u8 borderB = (color >> 8) & 0xFF;

    for (u32 y = 0; y < targetSize; ++y) {
        for (u32 x = 0; x < targetSize; ++x) {
            u32 idx = (y * targetSize + x) * 4;
            bool isBorder = (x < 2 || x >= targetSize - 2 || y < 2 || y >= targetSize - 2);
            result.pixels[idx]     = isBorder ? borderR : bgR;
            result.pixels[idx + 1] = isBorder ? borderG : bgG;
            result.pixels[idx + 2] = isBorder ? borderB : bgB;
            result.pixels[idx + 3] = 255;
        }
    }

    // Center the label
    usize labelLen = std::strlen(label);
    i32 textW = static_cast<i32>(labelLen) * 6 - 1;
    i32 textX = static_cast<i32>(targetSize / 2) - textW / 2;
    i32 textY = static_cast<i32>(targetSize / 2) - 3;
    DrawLabel(result.pixels, targetSize, targetSize, label, textX, textY, color);

    result.valid = true;
    return result;
}

void ThumbnailGenerator::ClearCache() {
    m_Cache.clear();
}

bool ThumbnailGenerator::HasCachedThumbnail(const std::string& path) const {
    return m_Cache.find(path) != m_Cache.end();
}

const ThumbnailData& ThumbnailGenerator::GetCachedThumbnail(const std::string& path) const {
    auto it = m_Cache.find(path);
    if (it != m_Cache.end()) return it->second;
    return s_InvalidThumbnail;
}

} // namespace Assets
} // namespace Enjin
