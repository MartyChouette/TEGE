#include "Enjin/Assets/PLYLoader.h"
#include "Enjin/Logging/Log.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>

namespace Enjin {
namespace Assets {

std::string PLYLoader::s_LastError;

bool PLYLoader::Load(const std::string& filepath, PLYMesh& outMesh) {
    s_LastError.clear();
    outMesh = PLYMesh{};

    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        s_LastError = "Failed to open PLY file: " + filepath;
        ENJIN_LOG_ERROR(Asset, "%s", s_LastError.c_str());
        return false;
    }

    // Parse the header
    PLYHeader header;
    if (!ParseHeader(file, header)) {
        return false;
    }

    // Parse body based on format
    bool success = false;
    if (header.format == PLYFormat::ASCII) {
        success = ParseASCII(file, header, outMesh);
    } else {
        success = ParseBinary(file, header, outMesh);
    }

    return success;
}

const std::string& PLYLoader::GetLastError() {
    return s_LastError;
}

PLYLoader::PLYType PLYLoader::ResolveType(const std::string& typeName) {
    if (typeName == "char" || typeName == "int8")       return PLYType::Char;
    if (typeName == "uchar" || typeName == "uint8")     return PLYType::UChar;
    if (typeName == "short" || typeName == "int16")     return PLYType::Short;
    if (typeName == "ushort" || typeName == "uint16")   return PLYType::UShort;
    if (typeName == "int" || typeName == "int32")       return PLYType::Int;
    if (typeName == "uint" || typeName == "uint32")     return PLYType::UInt;
    if (typeName == "float" || typeName == "float32")   return PLYType::Float;
    if (typeName == "double" || typeName == "float64")  return PLYType::Double;
    return PLYType::Invalid;
}

u32 PLYLoader::TypeSize(PLYType type) {
    switch (type) {
        case PLYType::Char:   return 1;
        case PLYType::UChar:  return 1;
        case PLYType::Short:  return 2;
        case PLYType::UShort: return 2;
        case PLYType::Int:    return 4;
        case PLYType::UInt:   return 4;
        case PLYType::Float:  return 4;
        case PLYType::Double: return 8;
        default:              return 0;
    }
}

void PLYLoader::SwapBytes(void* data, u32 size) {
    u8* bytes = static_cast<u8*>(data);
    for (u32 i = 0; i < size / 2; ++i) {
        std::swap(bytes[i], bytes[size - 1 - i]);
    }
}

f64 PLYLoader::ReadBinaryValue(std::istream& stream, PLYType type, bool swapEndian) {
    switch (type) {
        case PLYType::Char: {
            i8 v; stream.read(reinterpret_cast<char*>(&v), 1);
            return static_cast<f64>(v);
        }
        case PLYType::UChar: {
            u8 v; stream.read(reinterpret_cast<char*>(&v), 1);
            return static_cast<f64>(v);
        }
        case PLYType::Short: {
            i16 v; stream.read(reinterpret_cast<char*>(&v), 2);
            if (swapEndian) SwapBytes(&v, 2);
            return static_cast<f64>(v);
        }
        case PLYType::UShort: {
            u16 v; stream.read(reinterpret_cast<char*>(&v), 2);
            if (swapEndian) SwapBytes(&v, 2);
            return static_cast<f64>(v);
        }
        case PLYType::Int: {
            i32 v; stream.read(reinterpret_cast<char*>(&v), 4);
            if (swapEndian) SwapBytes(&v, 4);
            return static_cast<f64>(v);
        }
        case PLYType::UInt: {
            u32 v; stream.read(reinterpret_cast<char*>(&v), 4);
            if (swapEndian) SwapBytes(&v, 4);
            return static_cast<f64>(v);
        }
        case PLYType::Float: {
            f32 v; stream.read(reinterpret_cast<char*>(&v), 4);
            if (swapEndian) SwapBytes(&v, 4);
            return static_cast<f64>(v);
        }
        case PLYType::Double: {
            f64 v; stream.read(reinterpret_cast<char*>(&v), 8);
            if (swapEndian) SwapBytes(&v, 8);
            return v;
        }
        default:
            return 0.0;
    }
}

bool PLYLoader::ParseHeader(std::istream& stream, PLYHeader& outHeader) {
    std::string line;

    // First line must be "ply"
    if (!std::getline(stream, line)) {
        s_LastError = "PLY file is empty";
        ENJIN_LOG_ERROR(Asset, "%s", s_LastError.c_str());
        return false;
    }
    // Strip carriage return if present
    if (!line.empty() && line.back() == '\r') line.pop_back();

    if (line != "ply") {
        s_LastError = "Not a valid PLY file (missing 'ply' magic)";
        ENJIN_LOG_ERROR(Asset, "%s", s_LastError.c_str());
        return false;
    }

    PLYElement* currentElement = nullptr;

    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        std::istringstream iss(line);
        std::string keyword;
        iss >> keyword;

        if (keyword == "format") {
            std::string formatStr, version;
            iss >> formatStr >> version;

            if (formatStr == "ascii") {
                outHeader.format = PLYFormat::ASCII;
            } else if (formatStr == "binary_little_endian") {
                outHeader.format = PLYFormat::BinaryLittleEndian;
            } else if (formatStr == "binary_big_endian") {
                outHeader.format = PLYFormat::BinaryBigEndian;
            } else {
                s_LastError = "Unsupported PLY format: " + formatStr;
                ENJIN_LOG_ERROR(Asset, "%s", s_LastError.c_str());
                return false;
            }
        } else if (keyword == "element") {
            PLYElement element;
            iss >> element.name >> element.count;
            outHeader.elements.push_back(element);
            currentElement = &outHeader.elements.back();
        } else if (keyword == "property") {
            if (!currentElement) {
                s_LastError = "PLY property found before any element";
                ENJIN_LOG_ERROR(Asset, "%s", s_LastError.c_str());
                return false;
            }

            PLYProperty prop;
            std::string typeStr;
            iss >> typeStr;

            if (typeStr == "list") {
                prop.isList = true;
                std::string countTypeStr, valueTypeStr;
                iss >> countTypeStr >> valueTypeStr >> prop.name;
                prop.listCountType = ResolveType(countTypeStr);
                prop.listValueType = ResolveType(valueTypeStr);

                if (prop.listCountType == PLYType::Invalid || prop.listValueType == PLYType::Invalid) {
                    s_LastError = "Invalid list property types in PLY header";
                    ENJIN_LOG_ERROR(Asset, "%s", s_LastError.c_str());
                    return false;
                }
            } else {
                prop.isList = false;
                prop.type = ResolveType(typeStr);
                iss >> prop.name;

                if (prop.type == PLYType::Invalid) {
                    s_LastError = "Unknown property type '" + typeStr + "' in PLY header";
                    ENJIN_LOG_ERROR(Asset, "%s", s_LastError.c_str());
                    return false;
                }
            }

            currentElement->properties.push_back(prop);
        } else if (keyword == "end_header") {
            break;
        }
        // Ignore comment lines and other keywords
    }

    if (outHeader.elements.empty()) {
        s_LastError = "PLY header contains no elements";
        ENJIN_LOG_ERROR(Asset, "%s", s_LastError.c_str());
        return false;
    }

    return true;
}

bool PLYLoader::ParseASCII(std::istream& stream, const PLYHeader& header, PLYMesh& outMesh) {
    for (const auto& element : header.elements) {
        if (element.name == "vertex") {
            // Determine property indices
            i32 xIdx = -1, yIdx = -1, zIdx = -1;
            i32 nxIdx = -1, nyIdx = -1, nzIdx = -1;
            i32 rIdx = -1, gIdx = -1, bIdx = -1;
            i32 sIdx = -1, tIdx = -1;

            for (i32 i = 0; i < static_cast<i32>(element.properties.size()); ++i) {
                const auto& name = element.properties[i].name;
                if (name == "x") xIdx = i;
                else if (name == "y") yIdx = i;
                else if (name == "z") zIdx = i;
                else if (name == "nx") nxIdx = i;
                else if (name == "ny") nyIdx = i;
                else if (name == "nz") nzIdx = i;
                else if (name == "red") rIdx = i;
                else if (name == "green") gIdx = i;
                else if (name == "blue") bIdx = i;
                else if (name == "s" || name == "texture_u" || name == "u") sIdx = i;
                else if (name == "t" || name == "texture_v" || name == "v") tIdx = i;
            }

            if (xIdx < 0 || yIdx < 0 || zIdx < 0) {
                s_LastError = "PLY vertex element missing x/y/z properties";
                ENJIN_LOG_ERROR(Asset, "%s", s_LastError.c_str());
                return false;
            }

            outMesh.hasNormals = (nxIdx >= 0 && nyIdx >= 0 && nzIdx >= 0);
            outMesh.hasColors = (rIdx >= 0 && gIdx >= 0 && bIdx >= 0);
            outMesh.hasTexCoords = (sIdx >= 0 && tIdx >= 0);

            outMesh.vertices.reserve(element.count);

            for (u32 v = 0; v < element.count; ++v) {
                std::string line;
                if (!std::getline(stream, line)) {
                    s_LastError = "Unexpected end of file reading PLY vertices";
                    ENJIN_LOG_ERROR(Asset, "%s", s_LastError.c_str());
                    return false;
                }
                if (!line.empty() && line.back() == '\r') line.pop_back();

                std::istringstream iss(line);
                std::vector<f64> values;
                values.reserve(element.properties.size());

                f64 val;
                while (iss >> val) {
                    values.push_back(val);
                }

                if (values.size() < element.properties.size()) {
                    s_LastError = "PLY vertex line has too few values at vertex " + std::to_string(v);
                    ENJIN_LOG_ERROR(Asset, "%s", s_LastError.c_str());
                    return false;
                }

                PLYVertex vertex;
                vertex.position = Math::Vector3(
                    static_cast<f32>(values[xIdx]),
                    static_cast<f32>(values[yIdx]),
                    static_cast<f32>(values[zIdx])
                );

                if (outMesh.hasNormals) {
                    vertex.hasNormal = true;
                    vertex.normal = Math::Vector3(
                        static_cast<f32>(values[nxIdx]),
                        static_cast<f32>(values[nyIdx]),
                        static_cast<f32>(values[nzIdx])
                    );
                }

                if (outMesh.hasColors) {
                    vertex.hasColor = true;
                    // Colors in PLY are typically 0-255 integers; normalize to 0-1
                    vertex.color = Math::Vector3(
                        static_cast<f32>(values[rIdx]) / 255.0f,
                        static_cast<f32>(values[gIdx]) / 255.0f,
                        static_cast<f32>(values[bIdx]) / 255.0f
                    );
                }

                if (outMesh.hasTexCoords) {
                    vertex.hasTexCoord = true;
                    vertex.texCoord = Math::Vector2(
                        static_cast<f32>(values[sIdx]),
                        static_cast<f32>(values[tIdx])
                    );
                }

                outMesh.vertices.push_back(vertex);
            }
        } else if (element.name == "face") {
            // Find the vertex_indices (or vertex_index) list property
            i32 indicesPropertyIdx = -1;
            for (i32 i = 0; i < static_cast<i32>(element.properties.size()); ++i) {
                const auto& prop = element.properties[i];
                if (prop.isList && (prop.name == "vertex_indices" || prop.name == "vertex_index")) {
                    indicesPropertyIdx = i;
                    break;
                }
            }

            if (indicesPropertyIdx < 0) {
                s_LastError = "PLY face element has no vertex_indices list property";
                ENJIN_LOG_ERROR(Asset, "%s", s_LastError.c_str());
                return false;
            }

            outMesh.indices.reserve(element.count * 3);

            for (u32 f = 0; f < element.count; ++f) {
                std::string line;
                if (!std::getline(stream, line)) {
                    s_LastError = "Unexpected end of file reading PLY faces";
                    ENJIN_LOG_ERROR(Asset, "%s", s_LastError.c_str());
                    return false;
                }
                if (!line.empty() && line.back() == '\r') line.pop_back();

                std::istringstream iss(line);

                // Skip properties before the indices property
                for (i32 p = 0; p < indicesPropertyIdx; ++p) {
                    const auto& prop = element.properties[p];
                    if (prop.isList) {
                        u32 listCount;
                        iss >> listCount;
                        f64 dummy;
                        for (u32 j = 0; j < listCount; ++j) iss >> dummy;
                    } else {
                        f64 dummy;
                        iss >> dummy;
                    }
                }

                // Read the face vertex count and indices
                u32 vertexCount;
                iss >> vertexCount;

                if (vertexCount < 3) {
                    s_LastError = "PLY face with fewer than 3 vertices at face " + std::to_string(f);
                    ENJIN_LOG_ERROR(Asset, "%s", s_LastError.c_str());
                    return false;
                }

                std::vector<u32> faceIndices(vertexCount);
                for (u32 j = 0; j < vertexCount; ++j) {
                    iss >> faceIndices[j];
                }

                // Triangulate using fan triangulation (works for convex polygons)
                for (u32 j = 1; j + 1 < vertexCount; ++j) {
                    outMesh.indices.push_back(faceIndices[0]);
                    outMesh.indices.push_back(faceIndices[j]);
                    outMesh.indices.push_back(faceIndices[j + 1]);
                }
            }
        } else {
            // Unknown element - skip its lines
            for (u32 i = 0; i < element.count; ++i) {
                std::string line;
                if (!std::getline(stream, line)) break;
            }
        }
    }

    return true;
}

bool PLYLoader::ParseBinary(std::istream& stream, const PLYHeader& header, PLYMesh& outMesh) {
    bool swapEndian = (header.format == PLYFormat::BinaryBigEndian);

    for (const auto& element : header.elements) {
        if (element.name == "vertex") {
            // Determine property indices
            i32 xIdx = -1, yIdx = -1, zIdx = -1;
            i32 nxIdx = -1, nyIdx = -1, nzIdx = -1;
            i32 rIdx = -1, gIdx = -1, bIdx = -1;
            i32 sIdx = -1, tIdx = -1;

            for (i32 i = 0; i < static_cast<i32>(element.properties.size()); ++i) {
                const auto& name = element.properties[i].name;
                if (name == "x") xIdx = i;
                else if (name == "y") yIdx = i;
                else if (name == "z") zIdx = i;
                else if (name == "nx") nxIdx = i;
                else if (name == "ny") nyIdx = i;
                else if (name == "nz") nzIdx = i;
                else if (name == "red") rIdx = i;
                else if (name == "green") gIdx = i;
                else if (name == "blue") bIdx = i;
                else if (name == "s" || name == "texture_u" || name == "u") sIdx = i;
                else if (name == "t" || name == "texture_v" || name == "v") tIdx = i;
            }

            if (xIdx < 0 || yIdx < 0 || zIdx < 0) {
                s_LastError = "PLY vertex element missing x/y/z properties";
                ENJIN_LOG_ERROR(Asset, "%s", s_LastError.c_str());
                return false;
            }

            outMesh.hasNormals = (nxIdx >= 0 && nyIdx >= 0 && nzIdx >= 0);
            outMesh.hasColors = (rIdx >= 0 && gIdx >= 0 && bIdx >= 0);
            outMesh.hasTexCoords = (sIdx >= 0 && tIdx >= 0);

            outMesh.vertices.reserve(element.count);

            for (u32 v = 0; v < element.count; ++v) {
                std::vector<f64> values(element.properties.size());
                for (u32 p = 0; p < static_cast<u32>(element.properties.size()); ++p) {
                    const auto& prop = element.properties[p];
                    if (prop.isList) {
                        // Lists in vertex elements are unusual but we handle them
                        u32 listCount = static_cast<u32>(ReadBinaryValue(stream, prop.listCountType, swapEndian));
                        for (u32 j = 0; j < listCount; ++j) {
                            ReadBinaryValue(stream, prop.listValueType, swapEndian);
                        }
                        values[p] = 0.0;
                    } else {
                        values[p] = ReadBinaryValue(stream, prop.type, swapEndian);
                    }
                }

                if (!stream.good()) {
                    s_LastError = "Unexpected end of binary PLY data at vertex " + std::to_string(v);
                    ENJIN_LOG_ERROR(Asset, "%s", s_LastError.c_str());
                    return false;
                }

                PLYVertex vertex;
                vertex.position = Math::Vector3(
                    static_cast<f32>(values[xIdx]),
                    static_cast<f32>(values[yIdx]),
                    static_cast<f32>(values[zIdx])
                );

                if (outMesh.hasNormals) {
                    vertex.hasNormal = true;
                    vertex.normal = Math::Vector3(
                        static_cast<f32>(values[nxIdx]),
                        static_cast<f32>(values[nyIdx]),
                        static_cast<f32>(values[nzIdx])
                    );
                }

                if (outMesh.hasColors) {
                    vertex.hasColor = true;
                    // Binary PLY colors are usually 0-255 uchar
                    vertex.color = Math::Vector3(
                        static_cast<f32>(values[rIdx]) / 255.0f,
                        static_cast<f32>(values[gIdx]) / 255.0f,
                        static_cast<f32>(values[bIdx]) / 255.0f
                    );
                }

                if (outMesh.hasTexCoords) {
                    vertex.hasTexCoord = true;
                    vertex.texCoord = Math::Vector2(
                        static_cast<f32>(values[sIdx]),
                        static_cast<f32>(values[tIdx])
                    );
                }

                outMesh.vertices.push_back(vertex);
            }
        } else if (element.name == "face") {
            // Find the vertex_indices list property
            i32 indicesPropertyIdx = -1;
            for (i32 i = 0; i < static_cast<i32>(element.properties.size()); ++i) {
                const auto& prop = element.properties[i];
                if (prop.isList && (prop.name == "vertex_indices" || prop.name == "vertex_index")) {
                    indicesPropertyIdx = i;
                    break;
                }
            }

            if (indicesPropertyIdx < 0) {
                s_LastError = "PLY face element has no vertex_indices list property";
                ENJIN_LOG_ERROR(Asset, "%s", s_LastError.c_str());
                return false;
            }

            outMesh.indices.reserve(element.count * 3);

            for (u32 f = 0; f < element.count; ++f) {
                // Skip properties before the indices property
                for (i32 p = 0; p < indicesPropertyIdx; ++p) {
                    const auto& prop = element.properties[p];
                    if (prop.isList) {
                        u32 listCount = static_cast<u32>(ReadBinaryValue(stream, prop.listCountType, swapEndian));
                        for (u32 j = 0; j < listCount; ++j) {
                            ReadBinaryValue(stream, prop.listValueType, swapEndian);
                        }
                    } else {
                        ReadBinaryValue(stream, prop.type, swapEndian);
                    }
                }

                // Read vertex count for this face
                const auto& indicesProp = element.properties[indicesPropertyIdx];
                u32 vertexCount = static_cast<u32>(
                    ReadBinaryValue(stream, indicesProp.listCountType, swapEndian)
                );

                if (vertexCount < 3) {
                    s_LastError = "PLY binary face with fewer than 3 vertices at face " + std::to_string(f);
                    ENJIN_LOG_ERROR(Asset, "%s", s_LastError.c_str());
                    return false;
                }

                std::vector<u32> faceIndices(vertexCount);
                for (u32 j = 0; j < vertexCount; ++j) {
                    faceIndices[j] = static_cast<u32>(
                        ReadBinaryValue(stream, indicesProp.listValueType, swapEndian)
                    );
                }

                // Skip properties after the indices property
                for (i32 p = indicesPropertyIdx + 1; p < static_cast<i32>(element.properties.size()); ++p) {
                    const auto& prop = element.properties[p];
                    if (prop.isList) {
                        u32 listCount = static_cast<u32>(ReadBinaryValue(stream, prop.listCountType, swapEndian));
                        for (u32 j = 0; j < listCount; ++j) {
                            ReadBinaryValue(stream, prop.listValueType, swapEndian);
                        }
                    } else {
                        ReadBinaryValue(stream, prop.type, swapEndian);
                    }
                }

                if (!stream.good()) {
                    s_LastError = "Unexpected end of binary PLY data at face " + std::to_string(f);
                    ENJIN_LOG_ERROR(Asset, "%s", s_LastError.c_str());
                    return false;
                }

                // Fan triangulation
                for (u32 j = 1; j + 1 < vertexCount; ++j) {
                    outMesh.indices.push_back(faceIndices[0]);
                    outMesh.indices.push_back(faceIndices[j]);
                    outMesh.indices.push_back(faceIndices[j + 1]);
                }
            }
        } else {
            // Unknown element - skip its binary data
            for (u32 i = 0; i < element.count; ++i) {
                for (const auto& prop : element.properties) {
                    if (prop.isList) {
                        u32 listCount = static_cast<u32>(ReadBinaryValue(stream, prop.listCountType, swapEndian));
                        for (u32 j = 0; j < listCount; ++j) {
                            ReadBinaryValue(stream, prop.listValueType, swapEndian);
                        }
                    } else {
                        ReadBinaryValue(stream, prop.type, swapEndian);
                    }
                }
                if (!stream.good()) break;
            }
        }
    }

    return true;
}

} // namespace Assets
} // namespace Enjin
