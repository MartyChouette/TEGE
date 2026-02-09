#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include <vector>
#include <string>
#include <array>
#include <istream>

namespace Enjin {
namespace Assets {

// Single voxel in a MagicaVoxel model
struct VOXVoxel {
    u8 x, y, z;
    u8 colorIndex;
};

// Raw voxel model data from a .vox file
struct VOXModel {
    u32 sizeX = 0, sizeY = 0, sizeZ = 0;
    std::vector<VOXVoxel> voxels;
    std::array<u32, 256> palette;   // RGBA packed colors (0xAABBGGRR)
    bool hasCustomPalette = false;
};

// Mesh vertex generated from voxel data
struct VOXMeshVertex {
    Math::Vector3 position;
    Math::Vector3 normal;
    Math::Vector3 color;    // RGB in 0-1 range
};

// Triangle mesh generated from voxel model via greedy meshing
struct VOXMesh {
    std::vector<VOXMeshVertex> vertices;
    std::vector<u32> indices;
};

// MagicaVoxel .vox format loader
class ENJIN_API VOXLoader {
public:
    // Load raw voxel data from a .vox file
    static bool Load(const std::string& filepath, VOXModel& outModel);

    // Load a .vox file and convert directly to a triangle mesh
    static bool LoadAsMesh(const std::string& filepath, VOXMesh& outMesh);

    // Convert a voxel model to a triangle mesh with greedy face merging
    static void GenerateMesh(const VOXModel& model, VOXMesh& outMesh);

    // Get the last error message
    static const std::string& GetLastError();

private:
    static std::string s_LastError;

    // Initialize the default MagicaVoxel palette
    static void InitDefaultPalette(std::array<u32, 256>& palette);

    // Parse a chunk from the .vox binary stream
    // Returns false on read error; outChunkId/size/childSize are populated
    static bool ParseChunkHeader(std::istream& stream, char outChunkId[4],
                                 u32& outSize, u32& outChildSize);

    // Extract RGBA color components from a packed palette entry
    static Math::Vector3 PaletteToColor(u32 rgba);

    // Emit a quad (two triangles) for a single face during mesh generation
    static void EmitQuad(VOXMesh& mesh,
                         const Math::Vector3& v0, const Math::Vector3& v1,
                         const Math::Vector3& v2, const Math::Vector3& v3,
                         const Math::Vector3& normal, const Math::Vector3& color);
};

} // namespace Assets
} // namespace Enjin
