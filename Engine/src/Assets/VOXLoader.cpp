#include "Enjin/Assets/VOXLoader.h"
#include "Enjin/Logging/Log.h"
#include <fstream>
#include <cstring>
#include <unordered_map>
#include <unordered_set>

namespace Enjin {
namespace Assets {

std::string VOXLoader::s_LastError;

bool VOXLoader::Load(const std::string& filepath, VOXModel& outModel) {
    s_LastError.clear();
    outModel = VOXModel{};

    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        s_LastError = "Failed to open VOX file: " + filepath;
        ENJIN_LOG_ERROR(Asset, "%s", s_LastError.c_str());
        return false;
    }

    // Read magic number "VOX "
    char magic[4];
    file.read(magic, 4);
    if (!file.good() || std::memcmp(magic, "VOX ", 4) != 0) {
        s_LastError = "Not a valid MagicaVoxel file (bad magic): " + filepath;
        ENJIN_LOG_ERROR(Asset, "%s", s_LastError.c_str());
        return false;
    }

    // Read version (expected 150)
    u32 version;
    file.read(reinterpret_cast<char*>(&version), 4);
    if (!file.good()) {
        s_LastError = "Failed to read VOX version";
        ENJIN_LOG_ERROR(Asset, "%s", s_LastError.c_str());
        return false;
    }

    if (version != 150 && version != 200) {
        ENJIN_LOG_WARN(Asset, "VOX file version %u (expected 150); attempting load anyway", version);
    }

    // Read MAIN chunk header
    char mainId[4];
    u32 mainSize, mainChildSize;
    if (!ParseChunkHeader(file, mainId, mainSize, mainChildSize)) {
        s_LastError = "Failed to read MAIN chunk header";
        ENJIN_LOG_ERROR(Asset, "%s", s_LastError.c_str());
        return false;
    }

    if (std::memcmp(mainId, "MAIN", 4) != 0) {
        s_LastError = "Expected MAIN chunk, got something else";
        ENJIN_LOG_ERROR(Asset, "%s", s_LastError.c_str());
        return false;
    }

    // Skip MAIN chunk's own content (should be 0 bytes)
    if (mainSize > 0) {
        file.seekg(mainSize, std::ios::cur);
    }

    // Initialize default palette (used if no RGBA chunk found)
    InitDefaultPalette(outModel.palette);

    // Parse child chunks
    bool hasSizeChunk = false;

    while (file.good() && file.peek() != EOF) {
        char chunkId[4];
        u32 chunkSize, childSize;
        if (!ParseChunkHeader(file, chunkId, chunkSize, childSize)) {
            break;  // End of file or read error
        }

        std::streampos chunkDataStart = file.tellg();

        if (std::memcmp(chunkId, "SIZE", 4) == 0) {
            // Model dimensions
            if (chunkSize < 12) {
                s_LastError = "SIZE chunk too small";
                ENJIN_LOG_ERROR(Asset, "%s", s_LastError.c_str());
                return false;
            }
            file.read(reinterpret_cast<char*>(&outModel.sizeX), 4);
            file.read(reinterpret_cast<char*>(&outModel.sizeY), 4);
            file.read(reinterpret_cast<char*>(&outModel.sizeZ), 4);
            hasSizeChunk = true;
        } else if (std::memcmp(chunkId, "XYZI", 4) == 0) {
            // Voxel data
            if (!hasSizeChunk) {
                s_LastError = "XYZI chunk found before SIZE chunk";
                ENJIN_LOG_ERROR(Asset, "%s", s_LastError.c_str());
                return false;
            }

            u32 numVoxels;
            file.read(reinterpret_cast<char*>(&numVoxels), 4);
            if (!file.good()) {
                s_LastError = "Failed to read voxel count";
                ENJIN_LOG_ERROR(Asset, "%s", s_LastError.c_str());
                return false;
            }

            // Reject unreasonably large voxel counts to prevent overflow and OOM
            if (numVoxels > 16000000) {
                s_LastError = "XYZI voxel count too large: " + std::to_string(numVoxels);
                ENJIN_LOG_ERROR(Asset, "%s", s_LastError.c_str());
                return false;
            }

            // Validate count against remaining chunk data (use u64 to prevent overflow)
            u64 expectedBytes = static_cast<u64>(numVoxels) * 4;
            if (chunkSize < 4 || expectedBytes > static_cast<u64>(chunkSize - 4)) {
                s_LastError = "XYZI voxel count exceeds chunk size";
                ENJIN_LOG_ERROR(Asset, "%s", s_LastError.c_str());
                return false;
            }

            outModel.voxels.resize(numVoxels);
            for (u32 i = 0; i < numVoxels; ++i) {
                u8 data[4];
                file.read(reinterpret_cast<char*>(data), 4);
                if (!file.good()) {
                    s_LastError = "Unexpected end of file reading voxel " + std::to_string(i);
                    ENJIN_LOG_ERROR(Asset, "%s", s_LastError.c_str());
                    return false;
                }
                outModel.voxels[i].x = data[0];
                outModel.voxels[i].y = data[1];
                outModel.voxels[i].z = data[2];
                outModel.voxels[i].colorIndex = data[3];
            }
        } else if (std::memcmp(chunkId, "RGBA", 4) == 0) {
            // Custom palette (256 colors, 4 bytes each)
            if (chunkSize < 1024) {
                s_LastError = "RGBA chunk too small for full palette";
                ENJIN_LOG_ERROR(Asset, "%s", s_LastError.c_str());
                return false;
            }

            // MagicaVoxel RGBA chunk stores palette indices 1-255 in entries 0-254,
            // then an unused entry at index 255. The vox color index is 1-based.
            for (u32 i = 0; i < 256; ++i) {
                u8 rgba[4];
                file.read(reinterpret_cast<char*>(rgba), 4);
                if (!file.good()) {
                    s_LastError = "Failed to read palette entry " + std::to_string(i);
                    ENJIN_LOG_ERROR(Asset, "%s", s_LastError.c_str());
                    return false;
                }
                // Store as 0xAABBGGRR (RGBA packed little-endian)
                outModel.palette[i] = static_cast<u32>(rgba[0])
                                    | (static_cast<u32>(rgba[1]) << 8)
                                    | (static_cast<u32>(rgba[2]) << 16)
                                    | (static_cast<u32>(rgba[3]) << 24);
            }
            outModel.hasCustomPalette = true;
        }
        // Skip any remaining chunk data we didn't consume + child chunks
        std::streampos chunkEnd = chunkDataStart + static_cast<std::streamoff>(chunkSize);
        file.seekg(chunkEnd);
    }

    if (outModel.voxels.empty()) {
        s_LastError = "No voxel data found in VOX file";
        ENJIN_LOG_ERROR(Asset, "%s", s_LastError.c_str());
        return false;
    }

    return true;
}

bool VOXLoader::LoadAsMesh(const std::string& filepath, VOXMesh& outMesh) {
    VOXModel model;
    if (!Load(filepath, model)) {
        return false;
    }
    GenerateMesh(model, outMesh);
    return true;
}

void VOXLoader::GenerateMesh(const VOXModel& model, VOXMesh& outMesh) {
    outMesh.vertices.clear();
    outMesh.indices.clear();

    if (model.voxels.empty()) return;

    // Build a 3D occupancy set for neighbor lookups
    // Key: x | (y << 8) | (z << 16) — works for models up to 256^3
    auto voxelKey = [](u8 x, u8 y, u8 z) -> u32 {
        return static_cast<u32>(x) | (static_cast<u32>(y) << 8) | (static_cast<u32>(z) << 16);
    };

    std::unordered_map<u32, u8> occupancy;  // key -> colorIndex
    occupancy.reserve(model.voxels.size());
    for (const auto& v : model.voxels) {
        occupancy[voxelKey(v.x, v.y, v.z)] = v.colorIndex;
    }

    // Face directions: +X, -X, +Y, -Y, +Z, -Z
    struct FaceDir {
        i32 dx, dy, dz;
        Math::Vector3 normal;
        // The four corners of the quad relative to voxel origin (x,y,z),
        // defined so the normal faces outward with correct winding order
    };

    // Each voxel occupies [x, x+1] x [y, y+1] x [z, z+1]
    // For greedy meshing we work per-slice. We define 6 face directions.
    // For simplicity and correctness, we do a basic per-voxel face emission
    // with greedy merging along two axes for each face direction.

    // Face definitions: neighbor offset, normal, and the two tangent axes
    // used for greedy merging (axis0, axis1) plus the fixed axis
    struct GreedyFace {
        i32 neighborDx, neighborDy, neighborDz;
        Math::Vector3 normal;
        // Tangent axes for the plane (indices into xyz: 0=x, 1=y, 2=z)
        i32 fixedAxis;     // axis perpendicular to face
        i32 axis0, axis1;  // two axes spanning the face plane
        f32 fixedOffset;   // 0.0 or 1.0 offset from voxel origin along fixedAxis
    };

    GreedyFace faces[6] = {
        { 1,  0,  0, Math::Vector3( 1, 0, 0), 0, 1, 2, 1.0f}, // +X
        {-1,  0,  0, Math::Vector3(-1, 0, 0), 0, 1, 2, 0.0f}, // -X
        { 0,  1,  0, Math::Vector3( 0, 1, 0), 1, 0, 2, 1.0f}, // +Y
        { 0, -1,  0, Math::Vector3( 0,-1, 0), 1, 0, 2, 0.0f}, // -Y
        { 0,  0,  1, Math::Vector3( 0, 0, 1), 2, 0, 1, 1.0f}, // +Z
        { 0,  0, -1, Math::Vector3( 0, 0,-1), 2, 0, 1, 0.0f}, // -Z
    };

    // Determine model bounds
    u32 maxDim[3] = { model.sizeX, model.sizeY, model.sizeZ };

    // Pre-estimate output size
    outMesh.vertices.reserve(model.voxels.size() * 4); // rough estimate
    outMesh.indices.reserve(model.voxels.size() * 6);

    // For each face direction, sweep slices along the fixed axis and greedily merge
    for (i32 faceIdx = 0; faceIdx < 6; ++faceIdx) {
        const auto& face = faces[faceIdx];
        u32 dimFixed = maxDim[face.fixedAxis];
        u32 dim0 = maxDim[face.axis0];
        u32 dim1 = maxDim[face.axis1];

        // For each slice along the fixed axis
        for (u32 sliceVal = 0; sliceVal < dimFixed; ++sliceVal) {
            // Build a 2D grid of which cells in this slice have an exposed face
            // with their color index (0 = no face)
            std::vector<u8> mask(dim0 * dim1, 0);

            for (const auto& vox : model.voxels) {
                u8 coords[3] = { vox.x, vox.y, vox.z };
                if (coords[face.fixedAxis] != sliceVal) continue;

                // Check neighbor
                i32 nx = static_cast<i32>(vox.x) + face.neighborDx;
                i32 ny = static_cast<i32>(vox.y) + face.neighborDy;
                i32 nz = static_cast<i32>(vox.z) + face.neighborDz;

                // If neighbor is out of bounds or empty, face is exposed
                bool exposed = false;
                if (nx < 0 || ny < 0 || nz < 0 ||
                    nx >= static_cast<i32>(maxDim[0]) ||
                    ny >= static_cast<i32>(maxDim[1]) ||
                    nz >= static_cast<i32>(maxDim[2])) {
                    exposed = true;
                } else {
                    exposed = (occupancy.find(voxelKey(
                        static_cast<u8>(nx), static_cast<u8>(ny), static_cast<u8>(nz)
                    )) == occupancy.end());
                }

                if (exposed) {
                    u32 u = coords[face.axis0];
                    u32 v = coords[face.axis1];
                    mask[v * dim0 + u] = vox.colorIndex;
                }
            }

            // Greedy merge: scan the 2D mask for rectangular regions of same color
            std::vector<bool> visited(dim0 * dim1, false);

            for (u32 v1 = 0; v1 < dim1; ++v1) {
                for (u32 u0 = 0; u0 < dim0; ++u0) {
                    u32 idx = v1 * dim0 + u0;
                    if (mask[idx] == 0 || visited[idx]) continue;

                    u8 colorIdx = mask[idx];

                    // Expand along axis0 (u direction)
                    u32 width = 1;
                    while (u0 + width < dim0) {
                        u32 testIdx = v1 * dim0 + (u0 + width);
                        if (mask[testIdx] == colorIdx && !visited[testIdx]) {
                            ++width;
                        } else {
                            break;
                        }
                    }

                    // Expand along axis1 (v direction)
                    u32 height = 1;
                    bool canExpand = true;
                    while (canExpand && v1 + height < dim1) {
                        for (u32 du = 0; du < width; ++du) {
                            u32 testIdx = (v1 + height) * dim0 + (u0 + du);
                            if (mask[testIdx] != colorIdx || visited[testIdx]) {
                                canExpand = false;
                                break;
                            }
                        }
                        if (canExpand) ++height;
                    }

                    // Mark visited
                    for (u32 dv = 0; dv < height; ++dv) {
                        for (u32 du = 0; du < width; ++du) {
                            visited[(v1 + dv) * dim0 + (u0 + du)] = true;
                        }
                    }

                    // Build quad corners
                    // The quad spans [u0, u0+width] x [v1, v1+height] on the face plane,
                    // at the fixed coordinate sliceVal + fixedOffset
                    f32 fixedCoord = static_cast<f32>(sliceVal) + face.fixedOffset;
                    f32 uMin = static_cast<f32>(u0);
                    f32 uMax = static_cast<f32>(u0 + width);
                    f32 vMin = static_cast<f32>(v1);
                    f32 vMax = static_cast<f32>(v1 + height);

                    // Build 4 corner positions
                    auto makePos = [&](f32 uVal, f32 vVal) -> Math::Vector3 {
                        f32 coords3[3];
                        coords3[face.fixedAxis] = fixedCoord;
                        coords3[face.axis0] = uVal;
                        coords3[face.axis1] = vVal;
                        return Math::Vector3(coords3[0], coords3[1], coords3[2]);
                    };

                    Math::Vector3 c0 = makePos(uMin, vMin);
                    Math::Vector3 c1 = makePos(uMax, vMin);
                    Math::Vector3 c2 = makePos(uMax, vMax);
                    Math::Vector3 c3 = makePos(uMin, vMax);

                    // Palette color lookup: MagicaVoxel color indices are 1-based,
                    // mapping to palette entries 0-254
                    Math::Vector3 color = PaletteToColor(
                        model.palette[colorIdx > 0 ? colorIdx - 1 : 0]
                    );

                    EmitQuad(outMesh, c0, c1, c2, c3, face.normal, color);
                }
            }
        }
    }
}

const std::string& VOXLoader::GetLastError() {
    return s_LastError;
}

void VOXLoader::InitDefaultPalette(std::array<u32, 256>& palette) {
    // Default MagicaVoxel palette (simplified gradient version)
    // The official default palette has specific artistic colors.
    // This provides a reasonable fallback with a hue/saturation/value spread.
    static const u32 defaultPalette[256] = {
        0x00000000, 0xFFFFFFFF, 0xFFCCFFFF, 0xFF99FFFF, 0xFF66FFFF, 0xFF33FFFF, 0xFF00FFFF, 0xFFFFCCFF,
        0xFFCCCCFF, 0xFF99CCFF, 0xFF66CCFF, 0xFF33CCFF, 0xFF00CCFF, 0xFFFF99FF, 0xFFCC99FF, 0xFF9999FF,
        0xFF6699FF, 0xFF3399FF, 0xFF0099FF, 0xFFFF66FF, 0xFFCC66FF, 0xFF9966FF, 0xFF6666FF, 0xFF3366FF,
        0xFF0066FF, 0xFFFF33FF, 0xFFCC33FF, 0xFF9933FF, 0xFF6633FF, 0xFF3333FF, 0xFF0033FF, 0xFFFF00FF,
        0xFFCC00FF, 0xFF9900FF, 0xFF6600FF, 0xFF3300FF, 0xFF0000FF, 0xFFFFFFCC, 0xFFCCFFCC, 0xFF99FFCC,
        0xFF66FFCC, 0xFF33FFCC, 0xFF00FFCC, 0xFFFFCCCC, 0xFFCCCCCC, 0xFF99CCCC, 0xFF66CCCC, 0xFF33CCCC,
        0xFF00CCCC, 0xFFFF99CC, 0xFFCC99CC, 0xFF9999CC, 0xFF6699CC, 0xFF3399CC, 0xFF0099CC, 0xFFFF66CC,
        0xFFCC66CC, 0xFF9966CC, 0xFF6666CC, 0xFF3366CC, 0xFF0066CC, 0xFFFF33CC, 0xFFCC33CC, 0xFF9933CC,
        0xFF6633CC, 0xFF3333CC, 0xFF0033CC, 0xFFFF00CC, 0xFFCC00CC, 0xFF9900CC, 0xFF6600CC, 0xFF3300CC,
        0xFF0000CC, 0xFFFFFF99, 0xFFCCFF99, 0xFF99FF99, 0xFF66FF99, 0xFF33FF99, 0xFF00FF99, 0xFFFFCC99,
        0xFFCCCC99, 0xFF99CC99, 0xFF66CC99, 0xFF33CC99, 0xFF00CC99, 0xFFFF9999, 0xFFCC9999, 0xFF999999,
        0xFF669999, 0xFF339999, 0xFF009999, 0xFFFF6699, 0xFFCC6699, 0xFF996699, 0xFF666699, 0xFF336699,
        0xFF006699, 0xFFFF3399, 0xFFCC3399, 0xFF993399, 0xFF663399, 0xFF333399, 0xFF003399, 0xFFFF0099,
        0xFFCC0099, 0xFF990099, 0xFF660099, 0xFF330099, 0xFF000099, 0xFFFFFF66, 0xFFCCFF66, 0xFF99FF66,
        0xFF66FF66, 0xFF33FF66, 0xFF00FF66, 0xFFFFCC66, 0xFFCCCC66, 0xFF99CC66, 0xFF66CC66, 0xFF33CC66,
        0xFF00CC66, 0xFFFF9966, 0xFFCC9966, 0xFF999966, 0xFF669966, 0xFF339966, 0xFF009966, 0xFFFF6666,
        0xFFCC6666, 0xFF996666, 0xFF666666, 0xFF336666, 0xFF006666, 0xFFFF3366, 0xFFCC3366, 0xFF993366,
        0xFF663366, 0xFF333366, 0xFF003366, 0xFFFF0066, 0xFFCC0066, 0xFF990066, 0xFF660066, 0xFF330066,
        0xFF000066, 0xFFFFFF33, 0xFFCCFF33, 0xFF99FF33, 0xFF66FF33, 0xFF33FF33, 0xFF00FF33, 0xFFFFCC33,
        0xFFCCCC33, 0xFF99CC33, 0xFF66CC33, 0xFF33CC33, 0xFF00CC33, 0xFFFF9933, 0xFFCC9933, 0xFF999933,
        0xFF669933, 0xFF339933, 0xFF009933, 0xFFFF6633, 0xFFCC6633, 0xFF996633, 0xFF666633, 0xFF336633,
        0xFF006633, 0xFFFF3333, 0xFFCC3333, 0xFF993333, 0xFF663333, 0xFF333333, 0xFF003333, 0xFFFF0033,
        0xFFCC0033, 0xFF990033, 0xFF660033, 0xFF330033, 0xFF000033, 0xFFFFFF00, 0xFFCCFF00, 0xFF99FF00,
        0xFF66FF00, 0xFF33FF00, 0xFF00FF00, 0xFFFFCC00, 0xFFCCCC00, 0xFF99CC00, 0xFF66CC00, 0xFF33CC00,
        0xFF00CC00, 0xFFFF9900, 0xFFCC9900, 0xFF999900, 0xFF669900, 0xFF339900, 0xFF009900, 0xFFFF6600,
        0xFFCC6600, 0xFF996600, 0xFF666600, 0xFF336600, 0xFF006600, 0xFFFF3300, 0xFFCC3300, 0xFF993300,
        0xFF663300, 0xFF333300, 0xFF003300, 0xFFFF0000, 0xFFCC0000, 0xFF990000, 0xFF660000, 0xFF330000,
        0xFF0000EE, 0xFF0000DD, 0xFF0000BB, 0xFF0000AA, 0xFF000088, 0xFF000077, 0xFF000055, 0xFF000044,
        0xFF000022, 0xFF000011, 0xFF00EE00, 0xFF00DD00, 0xFF00BB00, 0xFF00AA00, 0xFF008800, 0xFF007700,
        0xFF005500, 0xFF004400, 0xFF002200, 0xFF001100, 0xFFEE0000, 0xFFDD0000, 0xFFBB0000, 0xFFAA0000,
        0xFF880000, 0xFF770000, 0xFF550000, 0xFF440000, 0xFF220000, 0xFF110000, 0xFFEEEEEE, 0xFFDDDDDD,
        0xFFBBBBBB, 0xFFAAAAAA, 0xFF888888, 0xFF777777, 0xFF555555, 0xFF444444, 0xFF222222, 0xFF111111,
    };

    std::memcpy(palette.data(), defaultPalette, sizeof(defaultPalette));
}

bool VOXLoader::ParseChunkHeader(std::istream& stream, char outChunkId[4],
                                  u32& outSize, u32& outChildSize) {
    stream.read(outChunkId, 4);
    if (!stream.good()) return false;

    stream.read(reinterpret_cast<char*>(&outSize), 4);
    if (!stream.good()) return false;

    stream.read(reinterpret_cast<char*>(&outChildSize), 4);
    if (!stream.good()) return false;

    return true;
}

Math::Vector3 VOXLoader::PaletteToColor(u32 rgba) {
    // Packed as 0xAABBGGRR (little-endian RGBA)
    f32 r = static_cast<f32>((rgba >>  0) & 0xFF) / 255.0f;
    f32 g = static_cast<f32>((rgba >>  8) & 0xFF) / 255.0f;
    f32 b = static_cast<f32>((rgba >> 16) & 0xFF) / 255.0f;
    return Math::Vector3(r, g, b);
}

void VOXLoader::EmitQuad(VOXMesh& mesh,
                          const Math::Vector3& v0, const Math::Vector3& v1,
                          const Math::Vector3& v2, const Math::Vector3& v3,
                          const Math::Vector3& normal, const Math::Vector3& color) {
    u32 baseIndex = static_cast<u32>(mesh.vertices.size());

    mesh.vertices.push_back({v0, normal, color});
    mesh.vertices.push_back({v1, normal, color});
    mesh.vertices.push_back({v2, normal, color});
    mesh.vertices.push_back({v3, normal, color});

    // Two triangles: 0-1-2, 0-2-3
    mesh.indices.push_back(baseIndex + 0);
    mesh.indices.push_back(baseIndex + 1);
    mesh.indices.push_back(baseIndex + 2);

    mesh.indices.push_back(baseIndex + 0);
    mesh.indices.push_back(baseIndex + 2);
    mesh.indices.push_back(baseIndex + 3);
}

} // namespace Assets
} // namespace Enjin
