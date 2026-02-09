#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include <vector>
#include <string>
#include <istream>

namespace Enjin {
namespace Assets {

// Per-vertex data from PLY file
struct PLYVertex {
    Math::Vector3 position;
    Math::Vector3 normal;
    Math::Vector3 color;    // vertex colors normalized to 0-1 range
    Math::Vector2 texCoord;
    bool hasNormal = false;
    bool hasColor = false;
    bool hasTexCoord = false;
};

// Complete mesh data loaded from a PLY file
struct PLYMesh {
    std::vector<PLYVertex> vertices;
    std::vector<u32> indices;
    bool hasNormals = false;
    bool hasColors = false;
    bool hasTexCoords = false;
};

// PLY format loader (ASCII and binary little-endian)
class ENJIN_API PLYLoader {
public:
    // Load a PLY file into mesh data
    static bool Load(const std::string& filepath, PLYMesh& outMesh);

    // Get the last error message
    static const std::string& GetLastError();

private:
    static std::string s_LastError;

    // PLY storage format
    enum class PLYFormat { ASCII, BinaryLittleEndian, BinaryBigEndian };

    // Property data type in the PLY header
    enum class PLYType : u8 {
        Char, UChar, Short, UShort, Int, UInt, Float, Double, Invalid
    };

    // Single property definition from the header
    struct PLYProperty {
        std::string name;
        PLYType type = PLYType::Invalid;
        bool isList = false;
        PLYType listCountType = PLYType::Invalid;  // type of the count field for lists
        PLYType listValueType = PLYType::Invalid;   // type of the value fields for lists
    };

    // Element definition from the header (e.g., vertex, face)
    struct PLYElement {
        std::string name;
        u32 count = 0;
        std::vector<PLYProperty> properties;
    };

    // Parsed header information
    struct PLYHeader {
        PLYFormat format = PLYFormat::ASCII;
        std::vector<PLYElement> elements;
    };

    // Parse the PLY header from an input stream
    static bool ParseHeader(std::istream& stream, PLYHeader& outHeader);

    // Parse ASCII vertex and face data
    static bool ParseASCII(std::istream& stream, const PLYHeader& header, PLYMesh& outMesh);

    // Parse binary (little-endian or big-endian) vertex and face data
    static bool ParseBinary(std::istream& stream, const PLYHeader& header, PLYMesh& outMesh);

    // Resolve a type name string to PLYType enum
    static PLYType ResolveType(const std::string& typeName);

    // Get the byte size of a PLYType
    static u32 TypeSize(PLYType type);

    // Read a single value from a binary stream and return as double
    static f64 ReadBinaryValue(std::istream& stream, PLYType type, bool swapEndian);

    // Swap bytes for big-endian conversion
    static void SwapBytes(void* data, u32 size);
};

} // namespace Assets
} // namespace Enjin
