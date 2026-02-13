#include "Enjin/Assets/SWFLoader.h"
#include "Enjin/Logging/Log.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <filesystem>

// zlib decompression for compressed SWF (CWS) is optional.
// Full support requires linking zlib; for now compressed SWFs log a warning.

namespace Enjin {
namespace Assets {

// ============================================================================
// BitReader implementation
// ============================================================================

u32 SWFLoader::BitReader::ReadBits(u8 count) {
    u32 result = 0;
    for (u8 i = 0; i < count; ++i) {
        if (bytePos >= size) return result;
        result = (result << 1) | ((data[bytePos] >> (7 - bitPos)) & 1);
        bitPos++;
        if (bitPos >= 8) {
            bitPos = 0;
            bytePos++;
        }
    }
    return result;
}

i32 SWFLoader::BitReader::ReadSignedBits(u8 count) {
    if (count == 0) return 0;
    u32 raw = ReadBits(count);
    // Sign extend
    if (raw & (1u << (count - 1))) {
        raw |= ~((1u << count) - 1);
    }
    return static_cast<i32>(raw);
}

u8 SWFLoader::BitReader::ReadU8() {
    AlignByte();
    if (bytePos >= size) return 0;
    return data[bytePos++];
}

u16 SWFLoader::BitReader::ReadU16() {
    AlignByte();
    if (bytePos + 2 > size) return 0;
    u16 val = static_cast<u16>(data[bytePos]) | (static_cast<u16>(data[bytePos + 1]) << 8);
    bytePos += 2;
    return val;
}

u32 SWFLoader::BitReader::ReadU32() {
    AlignByte();
    if (bytePos + 4 > size) return 0;
    u32 val = static_cast<u32>(data[bytePos]) |
              (static_cast<u32>(data[bytePos + 1]) << 8) |
              (static_cast<u32>(data[bytePos + 2]) << 16) |
              (static_cast<u32>(data[bytePos + 3]) << 24);
    bytePos += 4;
    return val;
}

i16 SWFLoader::BitReader::ReadI16() {
    return static_cast<i16>(ReadU16());
}

i32 SWFLoader::BitReader::ReadI32() {
    return static_cast<i32>(ReadU32());
}

f32 SWFLoader::BitReader::ReadFixed8() {
    i16 val = ReadI16();
    return static_cast<f32>(val) / 256.0f;
}

f32 SWFLoader::BitReader::ReadFixed16() {
    i32 val = ReadI32();
    return static_cast<f32>(val) / 65536.0f;
}

std::string SWFLoader::BitReader::ReadString() {
    AlignByte();
    std::string result;
    while (bytePos < size) {
        u8 c = data[bytePos++];
        if (c == 0) break;
        result += static_cast<char>(c);
    }
    return result;
}

void SWFLoader::BitReader::AlignByte() {
    if (bitPos > 0) {
        bitPos = 0;
        bytePos++;
    }
}

bool SWFLoader::BitReader::HasMore() const {
    return bytePos < size;
}

void SWFLoader::BitReader::Skip(usize bytes) {
    AlignByte();
    bytePos += bytes;
    if (bytePos > size) bytePos = size;
}

// ============================================================================
// Tag header reading
// ============================================================================

SWFLoader::TagHeader SWFLoader::ReadTagHeader(BitReader& reader) {
    TagHeader header;
    u16 tagAndLength = reader.ReadU16();
    header.type = tagAndLength >> 6;
    header.length = tagAndLength & 0x3F;
    if (header.length == 0x3F) {
        // Long tag
        header.length = reader.ReadU32();
    }
    return header;
}

// ============================================================================
// Struct reading
// ============================================================================

SWFRect SWFLoader::ReadRect(BitReader& reader) {
    u8 nBits = static_cast<u8>(reader.ReadBits(5));
    SWFRect rect;
    rect.xMin = reader.ReadSignedBits(nBits);
    rect.xMax = reader.ReadSignedBits(nBits);
    rect.yMin = reader.ReadSignedBits(nBits);
    rect.yMax = reader.ReadSignedBits(nBits);
    return rect;
}

SWFMatrix SWFLoader::ReadMatrix(BitReader& reader) {
    SWFMatrix m;

    // Scale
    bool hasScale = reader.ReadBits(1) != 0;
    if (hasScale) {
        u8 nBits = static_cast<u8>(reader.ReadBits(5));
        m.scaleX = static_cast<f32>(reader.ReadSignedBits(nBits)) / 65536.0f;
        m.scaleY = static_cast<f32>(reader.ReadSignedBits(nBits)) / 65536.0f;
    }

    // Rotate/Skew
    bool hasRotate = reader.ReadBits(1) != 0;
    if (hasRotate) {
        u8 nBits = static_cast<u8>(reader.ReadBits(5));
        m.rotateSkew0 = static_cast<f32>(reader.ReadSignedBits(nBits)) / 65536.0f;
        m.rotateSkew1 = static_cast<f32>(reader.ReadSignedBits(nBits)) / 65536.0f;
    }

    // Translate
    u8 nBits = static_cast<u8>(reader.ReadBits(5));
    m.translateX = static_cast<f32>(reader.ReadSignedBits(nBits)) / 20.0f;  // twips to pixels
    m.translateY = static_cast<f32>(reader.ReadSignedBits(nBits)) / 20.0f;

    return m;
}

SWFColorTransform SWFLoader::ReadColorTransform(BitReader& reader, bool hasAlpha) {
    SWFColorTransform ct;
    bool hasAdd = reader.ReadBits(1) != 0;
    bool hasMult = reader.ReadBits(1) != 0;
    u8 nBits = static_cast<u8>(reader.ReadBits(4));

    if (hasMult) {
        ct.redMultTerm = static_cast<f32>(reader.ReadSignedBits(nBits)) / 256.0f;
        ct.greenMultTerm = static_cast<f32>(reader.ReadSignedBits(nBits)) / 256.0f;
        ct.blueMultTerm = static_cast<f32>(reader.ReadSignedBits(nBits)) / 256.0f;
        if (hasAlpha) ct.alphaMultTerm = static_cast<f32>(reader.ReadSignedBits(nBits)) / 256.0f;
    }
    if (hasAdd) {
        ct.redAddTerm = reader.ReadSignedBits(nBits);
        ct.greenAddTerm = reader.ReadSignedBits(nBits);
        ct.blueAddTerm = reader.ReadSignedBits(nBits);
        if (hasAlpha) ct.alphaAddTerm = reader.ReadSignedBits(nBits);
    }
    return ct;
}

SWFColor SWFLoader::ReadRGB(BitReader& reader) {
    SWFColor c;
    c.r = reader.ReadU8();
    c.g = reader.ReadU8();
    c.b = reader.ReadU8();
    c.a = 255;
    return c;
}

SWFColor SWFLoader::ReadRGBA(BitReader& reader) {
    SWFColor c;
    c.r = reader.ReadU8();
    c.g = reader.ReadU8();
    c.b = reader.ReadU8();
    c.a = reader.ReadU8();
    return c;
}

// ============================================================================
// Shape parsing
// ============================================================================

std::vector<SWFFillStyle> SWFLoader::ReadFillStyles(BitReader& reader, u8 shapeVersion) {
    std::vector<SWFFillStyle> styles;
    u16 count = reader.ReadU8();
    if (count == 0xFF && shapeVersion >= 2) {
        count = reader.ReadU16();
    }

    for (u16 i = 0; i < count; ++i) {
        SWFFillStyle fs;
        u8 type = reader.ReadU8();

        if (type == 0x00) {
            // Solid fill
            fs.type = SWFFillType::Solid;
            fs.color = (shapeVersion >= 3) ? ReadRGBA(reader) : ReadRGB(reader);
        }
        else if (type == 0x10 || type == 0x12) {
            // Linear or radial gradient
            fs.type = (type == 0x10) ? SWFFillType::LinearGradient : SWFFillType::RadialGradient;
            ReadMatrix(reader);  // gradient matrix (skip for now)
            u8 numGradients = reader.ReadU8();
            for (u8 g = 0; g < numGradients; ++g) {
                u8 ratio = reader.ReadU8();
                SWFColor color = (shapeVersion >= 3) ? ReadRGBA(reader) : ReadRGB(reader);
                fs.gradientStops.push_back({ratio, color});
            }
        }
        else if (type >= 0x40 && type <= 0x43) {
            // Bitmap fill
            fs.type = SWFFillType::Bitmap;
            fs.bitmapId = reader.ReadU16();
            fs.bitmapMatrix = ReadMatrix(reader);
        }
        else {
            // Unknown fill type — use solid black as fallback
            fs.type = SWFFillType::Solid;
            fs.color = {0, 0, 0, 255};
        }

        styles.push_back(fs);
    }
    return styles;
}

std::vector<SWFLineStyle> SWFLoader::ReadLineStyles(BitReader& reader, u8 shapeVersion) {
    std::vector<SWFLineStyle> styles;
    u16 count = reader.ReadU8();
    if (count == 0xFF && shapeVersion >= 2) {
        count = reader.ReadU16();
    }

    for (u16 i = 0; i < count; ++i) {
        SWFLineStyle ls;
        ls.width = reader.ReadU16();
        ls.color = (shapeVersion >= 3) ? ReadRGBA(reader) : ReadRGB(reader);
        styles.push_back(ls);
    }
    return styles;
}

std::vector<SWFShapePath> SWFLoader::ReadShapeRecords(BitReader& reader, u8 /*shapeVersion*/,
                                                       i32 numFillBits, i32 numLineBits) {
    std::vector<SWFShapePath> paths;
    SWFShapePath currentPath;
    f32 curX = 0, curY = 0;

    u32 shapeRecordIter = 0;
    constexpr u32 kMaxShapeRecordIterations = 100000;
    while (true) {
        if (++shapeRecordIter > kMaxShapeRecordIterations) {
            break;
        }
        u32 typeFlag = reader.ReadBits(1);

        if (typeFlag == 0) {
            // Non-edge record
            u32 flags = reader.ReadBits(5);
            if (flags == 0) break; // End of shape

            // StateNewStyles (bit 4)
            // StateLineStyle (bit 3)
            // StateFillStyle1 (bit 2)
            // StateFillStyle0 (bit 1)
            // StateMoveTo (bit 0)

            if (flags & 0x01) {
                // MoveTo
                if (!currentPath.edges.empty()) {
                    paths.push_back(currentPath);
                    currentPath = SWFShapePath{};
                }
                u8 moveBits = static_cast<u8>(reader.ReadBits(5));
                curX = static_cast<f32>(reader.ReadSignedBits(moveBits)) / 20.0f;
                curY = static_cast<f32>(reader.ReadSignedBits(moveBits)) / 20.0f;
                currentPath.startX = curX;
                currentPath.startY = curY;
            }
            if (flags & 0x02) {
                currentPath.fillStyle0 = static_cast<i32>(reader.ReadBits(static_cast<u8>(numFillBits))) - 1;
            }
            if (flags & 0x04) {
                currentPath.fillStyle1 = static_cast<i32>(reader.ReadBits(static_cast<u8>(numFillBits))) - 1;
            }
            if (flags & 0x08) {
                currentPath.lineStyle = static_cast<i32>(reader.ReadBits(static_cast<u8>(numLineBits))) - 1;
            }
            if (flags & 0x10) {
                // New styles — skip for simplicity
                // In a full implementation, read new fill+line styles and update bit counts
                break;
            }
        }
        else {
            // Edge record
            u32 straightFlag = reader.ReadBits(1);
            u8 numBitsMinusTwo = static_cast<u8>(reader.ReadBits(4));
            u8 numBts = numBitsMinusTwo + 2;

            SWFShapeEdge edge;

            if (straightFlag) {
                // Straight edge
                edge.type = SWFShapeEdgeType::StraightLine;
                u32 generalLine = reader.ReadBits(1);
                if (generalLine) {
                    f32 dx = static_cast<f32>(reader.ReadSignedBits(numBts)) / 20.0f;
                    f32 dy = static_cast<f32>(reader.ReadSignedBits(numBts)) / 20.0f;
                    curX += dx;
                    curY += dy;
                } else {
                    u32 vertLine = reader.ReadBits(1);
                    if (vertLine) {
                        f32 dy = static_cast<f32>(reader.ReadSignedBits(numBts)) / 20.0f;
                        curY += dy;
                    } else {
                        f32 dx = static_cast<f32>(reader.ReadSignedBits(numBts)) / 20.0f;
                        curX += dx;
                    }
                }
                edge.ax = curX;
                edge.ay = curY;
            }
            else {
                // Curved edge (quadratic Bezier)
                edge.type = SWFShapeEdgeType::CurvedLine;
                f32 cx = static_cast<f32>(reader.ReadSignedBits(numBts)) / 20.0f;
                f32 cy = static_cast<f32>(reader.ReadSignedBits(numBts)) / 20.0f;
                f32 ax = static_cast<f32>(reader.ReadSignedBits(numBts)) / 20.0f;
                f32 ay = static_cast<f32>(reader.ReadSignedBits(numBts)) / 20.0f;
                edge.cx = curX + cx;
                edge.cy = curY + cy;
                curX = edge.cx + ax;
                curY = edge.cy + ay;
                edge.ax = curX;
                edge.ay = curY;
            }

            currentPath.edges.push_back(edge);
        }
    }

    if (!currentPath.edges.empty()) {
        paths.push_back(currentPath);
    }

    return paths;
}

SWFShape SWFLoader::ParseDefineShape(BitReader& reader, u8 shapeVersion) {
    SWFShape shape;
    shape.characterId = reader.ReadU16();
    shape.bounds = ReadRect(reader);
    reader.AlignByte();

    shape.fillStyles = ReadFillStyles(reader, shapeVersion);
    shape.lineStyles = ReadLineStyles(reader, shapeVersion);

    u8 numFillBits = static_cast<u8>(reader.ReadBits(4));
    u8 numLineBits = static_cast<u8>(reader.ReadBits(4));

    shape.paths = ReadShapeRecords(reader, shapeVersion, numFillBits, numLineBits);

    return shape;
}

// ============================================================================
// PlaceObject2/3 parsing
// ============================================================================

SWFPlaceObject SWFLoader::ParsePlaceObject2(BitReader& reader, u8 version) {
    SWFPlaceObject po;

    u8 flags = reader.ReadU8();
    u8 flags2 = 0;
    if (version >= 3) {
        flags2 = reader.ReadU8();
        (void)flags2;  // Extended flags for PlaceObject3
    }

    po.depth = reader.ReadU16();

    po.move = (flags & 0x01) != 0;
    po.hasCharacter = (flags & 0x02) != 0;
    po.hasMatrix = (flags & 0x04) != 0;
    po.hasColorTransform = (flags & 0x08) != 0;
    bool hasRatio = (flags & 0x10) != 0;
    po.hasName = (flags & 0x20) != 0;
    bool hasClipDepth = (flags & 0x40) != 0;
    // bit 7: hasClipActions

    if (po.hasCharacter) {
        po.characterId = reader.ReadU16();
    }
    if (po.hasMatrix) {
        po.matrix = ReadMatrix(reader);
        reader.AlignByte();
    }
    if (po.hasColorTransform) {
        po.colorTransform = ReadColorTransform(reader, true);
        reader.AlignByte();
    }
    if (hasRatio) {
        po.ratio = reader.ReadU16();
    }
    if (po.hasName) {
        po.name = reader.ReadString();
    }
    if (hasClipDepth) {
        po.clipDepth = reader.ReadU16();
    }

    return po;
}

// ============================================================================
// DefineSprite parsing
// ============================================================================

SWFSprite SWFLoader::ParseDefineSprite(BitReader& reader) {
    SWFSprite sprite;
    sprite.characterId = reader.ReadU16();
    sprite.frameCount = reader.ReadU16();

    SWFFrame currentFrame;
    u32 frameIdx = 0;

    while (reader.HasMore()) {
        TagHeader tag = ReadTagHeader(reader);
        usize tagEnd = reader.bytePos + tag.length;

        switch (tag.type) {
            case 0: // End
                if (!currentFrame.placements.empty() || !currentFrame.removals.empty() ||
                    currentFrame.hasLabel || currentFrame.hasAction) {
                    currentFrame.frameIndex = frameIdx;
                    sprite.frames.push_back(currentFrame);
                }
                reader.bytePos = tagEnd;
                return sprite;

            case 1: // ShowFrame
                currentFrame.frameIndex = frameIdx++;
                sprite.frames.push_back(currentFrame);
                currentFrame = SWFFrame{};
                break;

            case 26: // PlaceObject2
            {
                BitReader tagReader;
                tagReader.data = reader.data + reader.bytePos;
                tagReader.size = tag.length;
                currentFrame.placements.push_back(ParsePlaceObject2(tagReader, 2));
                break;
            }

            case 5: // RemoveObject
            case 28: // RemoveObject2
            {
                SWFRemoveObject ro;
                if (tag.type == 5) {
                    reader.ReadU16(); // characterId (ignored)
                }
                ro.depth = reader.ReadU16();
                currentFrame.removals.push_back(ro);
                reader.bytePos = tagEnd;
                continue;
            }

            case 43: // FrameLabel
                currentFrame.label.name = reader.ReadString();
                currentFrame.hasLabel = true;
                break;

            case 12: // DoAction
                currentFrame.hasAction = true;
                if (tag.length > 0) {
                    currentFrame.actionData.assign(
                        reader.data + reader.bytePos,
                        reader.data + reader.bytePos + tag.length
                    );
                }
                break;

            default:
                break;
        }

        reader.bytePos = tagEnd;
    }

    return sprite;
}

// ============================================================================
// Bitmap parsing (stubs — full JPEG/zlib decode requires external libs)
// ============================================================================

SWFBitmap SWFLoader::ParseDefineBitsJPEG(BitReader& reader, u8 jpegVersion, u32 tagLength) {
    SWFBitmap bmp;
    bmp.characterId = reader.ReadU16();
    bmp.format = "JPEG";

    u32 dataSize = tagLength - 2;
    if (jpegVersion >= 3) {
        u32 alphaOffset = reader.ReadU32();
        (void)alphaOffset;
        dataSize -= 4;
    }

    // Store raw JPEG data — actual decode left to stb_image at conversion time
    if (dataSize > 0 && reader.bytePos + dataSize <= reader.size) {
        bmp.pixels.assign(reader.data + reader.bytePos, reader.data + reader.bytePos + dataSize);
    }

    return bmp;
}

SWFBitmap SWFLoader::ParseDefineBitsLossless(BitReader& reader, u8 losslessVersion, u32 tagLength) {
    SWFBitmap bmp;
    bmp.characterId = reader.ReadU16();
    bmp.format = "Lossless";

    u8 bitmapFormat = reader.ReadU8();
    bmp.width = reader.ReadU16();
    bmp.height = reader.ReadU16();

    u32 headerSize = 2 + 1 + 2 + 2;  // characterId + format + width + height
    u8 colorTableSize = 0;
    if (bitmapFormat == 3) {
        colorTableSize = reader.ReadU8();
        headerSize += 1;
    }

    u32 compressedSize = tagLength - headerSize;
    (void)colorTableSize;
    (void)compressedSize;
    (void)losslessVersion;

    // Store dimensions — full zlib decode done at conversion time
    return bmp;
}

SWFText SWFLoader::ParseDefineText(BitReader& reader, u8 /*textVersion*/) {
    SWFText text;
    text.characterId = reader.ReadU16();
    text.bounds = ReadRect(reader);
    reader.AlignByte();
    ReadMatrix(reader); // text matrix
    reader.AlignByte();
    // Skip text records for now — complex glyph-based format
    text.text = "[Flash Text]";
    return text;
}

SWFSound SWFLoader::ParseDefineSound(BitReader& reader, u32 tagLength) {
    SWFSound snd;
    snd.characterId = reader.ReadU16();

    u8 flags = reader.ReadU8();
    u8 codecId = (flags >> 4) & 0x0F;
    u8 sampleRateCode = (flags >> 2) & 0x03;
    u8 sampleSize = (flags >> 1) & 0x01;
    snd.channels = (flags & 0x01) ? 2 : 1;
    snd.bitsPerSample = sampleSize ? 16 : 8;

    switch (sampleRateCode) {
        case 0: snd.sampleRate = 5512; break;
        case 1: snd.sampleRate = 11025; break;
        case 2: snd.sampleRate = 22050; break;
        case 3: snd.sampleRate = 44100; break;
    }

    switch (codecId) {
        case 0: snd.format = "Raw"; break;
        case 1: snd.format = "ADPCM"; break;
        case 2: snd.format = "MP3"; break;
        case 3: snd.format = "Raw_LE"; break;
        case 6: snd.format = "Nellymoser"; break;
        default: snd.format = "Unknown"; break;
    }

    /*u32 sampleCount =*/ reader.ReadU32();
    u32 headerSize = 2 + 1 + 4;
    u32 dataSize = tagLength - headerSize;
    if (dataSize > 0 && reader.bytePos + dataSize <= reader.size) {
        snd.data.assign(reader.data + reader.bytePos, reader.data + reader.bytePos + dataSize);
    }

    return snd;
}

// ============================================================================
// Decompression
// ============================================================================

bool SWFLoader::DecompressZlib(const u8* input, usize inputSize,
                                std::vector<u8>& output, usize expectedSize) {
    // Zlib decompression stub — compressed SWFs (CWS) not yet supported
    // without linking zlib. Only uncompressed SWFs (FWS) are parsed.
    (void)input; (void)inputSize; (void)output; (void)expectedSize;
    ENJIN_LOG_WARN(Asset, "SWF zlib decompression not available — only uncompressed SWFs (FWS) supported");
    return false;
}

// ============================================================================
// Main parse functions
// ============================================================================

SWFParseResult SWFLoader::Parse(const std::string& filepath) {
    SWFParseResult result;

    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        result.errorMessage = "Cannot open file: " + filepath;
        return result;
    }

    usize fileSize = static_cast<usize>(file.tellg());
    file.seekg(0);

    std::vector<u8> data(fileSize);
    file.read(reinterpret_cast<char*>(data.data()), fileSize);
    file.close();

    return ParseFromMemory(data.data(), data.size());
}

SWFParseResult SWFLoader::ParseFromMemory(const u8* data, usize size) {
    SWFParseResult result;

    if (size < 8) {
        result.errorMessage = "File too small to be a valid SWF";
        return result;
    }

    // Read signature
    char sig[4] = { static_cast<char>(data[0]), static_cast<char>(data[1]),
                    static_cast<char>(data[2]), 0 };

    bool isCompressed = false;
    bool isLZMA = false;

    if (sig[0] == 'F' && sig[1] == 'W' && sig[2] == 'S') {
        // Uncompressed
    } else if (sig[0] == 'C' && sig[1] == 'W' && sig[2] == 'S') {
        isCompressed = true;
    } else if (sig[0] == 'Z' && sig[1] == 'W' && sig[2] == 'S') {
        isLZMA = true;
    } else {
        result.errorMessage = "Invalid SWF signature";
        return result;
    }

    auto& doc = result.document;
    doc.version = data[3];
    doc.fileLength = static_cast<u32>(data[4]) | (static_cast<u32>(data[5]) << 8) |
                     (static_cast<u32>(data[6]) << 16) | (static_cast<u32>(data[7]) << 24);

    // Prepare decompressed data
    std::vector<u8> decompressed;
    const u8* bodyData = nullptr;
    usize bodySize = 0;

    if (isCompressed) {
        doc.compression = "zlib";
        doc.compressed = true;

        // First 8 bytes are header, rest is zlib compressed
        if (!DecompressZlib(data + 8, size - 8, decompressed, doc.fileLength - 8)) {
            result.errorMessage = "Failed to decompress zlib SWF";
            return result;
        }
        bodyData = decompressed.data();
        bodySize = decompressed.size();
    }
    else if (isLZMA) {
        doc.compression = "lzma";
        doc.compressed = true;
        result.errorMessage = "LZMA-compressed SWF not yet supported";
        return result;
    }
    else {
        doc.compression = "none";
        doc.compressed = false;
        bodyData = data + 8;
        bodySize = size - 8;
    }

    // Parse header fields from body
    BitReader reader;
    reader.data = bodyData;
    reader.size = bodySize;

    doc.frameSize = ReadRect(reader);
    reader.AlignByte();

    doc.stageWidth = static_cast<f32>(doc.frameSize.xMax - doc.frameSize.xMin) / 20.0f;
    doc.stageHeight = static_cast<f32>(doc.frameSize.yMax - doc.frameSize.yMin) / 20.0f;

    u16 frameRateRaw = reader.ReadU16();
    doc.frameRate = static_cast<f32>(frameRateRaw >> 8) + static_cast<f32>(frameRateRaw & 0xFF) / 256.0f;
    if (doc.frameRate < 1.0f) doc.frameRate = 24.0f;

    doc.frameCount = reader.ReadU16();

    // Parse tags
    SWFFrame currentFrame;
    u32 frameIdx = 0;

    while (reader.HasMore()) {
        TagHeader tag = ReadTagHeader(reader);
        usize tagEnd = reader.bytePos + tag.length;

        if (tagEnd > bodySize) {
            doc.warnings.push_back("Tag extends past end of file at byte " + std::to_string(reader.bytePos));
            break;
        }

        doc.tagCount++;

        // SWF tag type constants
        switch (tag.type) {
            case 0: // End
                goto done_parsing;

            case 1: // ShowFrame
                currentFrame.frameIndex = frameIdx++;
                doc.mainTimeline.push_back(currentFrame);
                currentFrame = SWFFrame{};
                break;

            case 2:  // DefineShape
            case 22: // DefineShape2
            case 32: // DefineShape3
            case 83: // DefineShape4
            {
                u8 sv = 1;
                if (tag.type == 22) sv = 2;
                else if (tag.type == 32) sv = 3;
                else if (tag.type == 83) sv = 4;

                BitReader tagReader;
                tagReader.data = reader.data + reader.bytePos;
                tagReader.size = tag.length;
                SWFShape shape = ParseDefineShape(tagReader, sv);
                doc.shapes[shape.characterId] = std::move(shape);
                break;
            }

            case 6:  // DefineBits (JPEG)
            case 21: // DefineBitsJPEG2
            case 35: // DefineBitsJPEG3
            case 90: // DefineBitsJPEG4
            {
                u8 jv = 1;
                if (tag.type == 21) jv = 2;
                else if (tag.type == 35) jv = 3;
                else if (tag.type == 90) jv = 4;

                BitReader tagReader;
                tagReader.data = reader.data + reader.bytePos;
                tagReader.size = tag.length;
                SWFBitmap bmp = ParseDefineBitsJPEG(tagReader, jv, tag.length);
                doc.bitmaps[bmp.characterId] = std::move(bmp);
                break;
            }

            case 20: // DefineBitsLossless
            case 36: // DefineBitsLossless2
            {
                u8 lv = (tag.type == 36) ? 2 : 1;
                BitReader tagReader;
                tagReader.data = reader.data + reader.bytePos;
                tagReader.size = tag.length;
                SWFBitmap bmp = ParseDefineBitsLossless(tagReader, lv, tag.length);
                doc.bitmaps[bmp.characterId] = std::move(bmp);
                break;
            }

            case 39: // DefineSprite
            {
                BitReader tagReader;
                tagReader.data = reader.data + reader.bytePos;
                tagReader.size = tag.length;
                SWFSprite sprite = ParseDefineSprite(tagReader);
                doc.sprites[sprite.characterId] = std::move(sprite);
                break;
            }

            case 26: // PlaceObject2
            case 70: // PlaceObject3
            {
                BitReader tagReader;
                tagReader.data = reader.data + reader.bytePos;
                tagReader.size = tag.length;
                u8 pv = (tag.type == 70) ? 3 : 2;
                currentFrame.placements.push_back(ParsePlaceObject2(tagReader, pv));
                break;
            }

            case 5:  // RemoveObject
            case 28: // RemoveObject2
            {
                SWFRemoveObject ro;
                if (tag.type == 5) {
                    reader.ReadU16(); // characterId (skip)
                }
                ro.depth = reader.ReadU16();
                currentFrame.removals.push_back(ro);
                reader.bytePos = tagEnd;
                continue;
            }

            case 9: // SetBackgroundColor
                doc.backgroundColor = ReadRGB(reader);
                break;

            case 43: // FrameLabel
                currentFrame.label.name = reader.ReadString();
                currentFrame.hasLabel = true;
                break;

            case 12: // DoAction (AS1/2)
                currentFrame.hasAction = true;
                if (tag.length > 0) {
                    currentFrame.actionData.assign(
                        reader.data + reader.bytePos,
                        reader.data + reader.bytePos + tag.length
                    );
                }
                break;

            case 59: // DoInitAction (AS2)
                // Skip characterId, store action bytes
                if (tag.length > 2) {
                    reader.ReadU16(); // spriteId
                    currentFrame.hasAction = true;
                    u32 actLen = tag.length - 2;
                    currentFrame.actionData.assign(
                        reader.data + reader.bytePos,
                        reader.data + reader.bytePos + actLen
                    );
                }
                break;

            case 82: // DoABC (AS3)
                currentFrame.hasAction = true;
                if (tag.length > 0) {
                    currentFrame.actionData.assign(
                        reader.data + reader.bytePos,
                        reader.data + reader.bytePos + tag.length
                    );
                }
                break;

            case 76: // SymbolClass
            case 56: // ExportAssets
            {
                u16 numSymbols = reader.ReadU16();
                for (u16 i = 0; i < numSymbols; ++i) {
                    u16 charId = reader.ReadU16();
                    std::string name = reader.ReadString();
                    doc.exports[charId] = name;
                    // Set sprite export name if applicable
                    auto it = doc.sprites.find(charId);
                    if (it != doc.sprites.end()) {
                        it->second.exportName = name;
                    }
                }
                break;
            }

            case 14: // DefineSound
            {
                BitReader tagReader;
                tagReader.data = reader.data + reader.bytePos;
                tagReader.size = tag.length;
                SWFSound snd = ParseDefineSound(tagReader, tag.length);
                doc.sounds[snd.characterId] = std::move(snd);
                break;
            }

            case 11: // DefineText
            case 33: // DefineText2
            {
                BitReader tagReader;
                tagReader.data = reader.data + reader.bytePos;
                tagReader.size = tag.length;
                u8 tv = (tag.type == 33) ? 2 : 1;
                SWFText txt = ParseDefineText(tagReader, tv);
                doc.texts[txt.characterId] = std::move(txt);
                break;
            }

            default:
                doc.unknownTagCount++;
                break;
        }

        reader.bytePos = tagEnd;
    }

done_parsing:
    // Flush any remaining frame
    if (!currentFrame.placements.empty() || !currentFrame.removals.empty()) {
        currentFrame.frameIndex = frameIdx;
        doc.mainTimeline.push_back(currentFrame);
    }

    result.success = true;
    ENJIN_LOG_INFO(Assets, "SWF parsed: v%u, %.0fx%.0f, %.1f fps, %u frames, "
                           "%u shapes, %u sprites, %u bitmaps, %u sounds, %u tags (%u unknown)",
                   doc.version, doc.stageWidth, doc.stageHeight, doc.frameRate,
                   static_cast<u32>(doc.mainTimeline.size()),
                   static_cast<u32>(doc.shapes.size()),
                   static_cast<u32>(doc.sprites.size()),
                   static_cast<u32>(doc.bitmaps.size()),
                   static_cast<u32>(doc.sounds.size()),
                   doc.tagCount, doc.unknownTagCount);

    return result;
}

// ============================================================================
// SVG conversion
// ============================================================================

std::string SWFLoader::ShapeToSVGPath(const SWFShape& shape) {
    std::string svg;

    for (auto& path : shape.paths) {
        char buf[128];
        snprintf(buf, sizeof(buf), "M%.1f %.1f", path.startX, path.startY);
        svg += buf;

        for (auto& edge : path.edges) {
            if (edge.type == SWFShapeEdgeType::StraightLine) {
                snprintf(buf, sizeof(buf), " L%.1f %.1f", edge.ax, edge.ay);
            } else {
                snprintf(buf, sizeof(buf), " Q%.1f %.1f %.1f %.1f",
                         edge.cx, edge.cy, edge.ax, edge.ay);
            }
            svg += buf;
        }
    }

    return svg;
}

std::string SWFLoader::ConvertToSVG(const SWFDocument& doc) {
    std::ostringstream svg;
    svg << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    svg << "<svg xmlns=\"http://www.w3.org/2000/svg\" "
        << "width=\"" << doc.stageWidth << "\" height=\"" << doc.stageHeight << "\" "
        << "viewBox=\"0 0 " << doc.stageWidth << " " << doc.stageHeight << "\">\n";

    // Background
    char bgBuf[32];
    snprintf(bgBuf, sizeof(bgBuf), "#%02x%02x%02x",
             doc.backgroundColor.r, doc.backgroundColor.g, doc.backgroundColor.b);
    svg << "  <rect width=\"100%\" height=\"100%\" fill=\"" << bgBuf << "\"/>\n";

    // Shapes
    for (auto& [id, shape] : doc.shapes) {
        std::string pathData = ShapeToSVGPath(shape);
        if (pathData.empty()) continue;

        std::string fill = "none";
        std::string stroke = "none";
        f32 strokeWidth = 1.0f;

        // Use first fill style if available
        for (auto& p : shape.paths) {
            if (p.fillStyle0 >= 0 && p.fillStyle0 < static_cast<i32>(shape.fillStyles.size())) {
                auto& fs = shape.fillStyles[p.fillStyle0];
                if (fs.type == SWFFillType::Solid) {
                    char buf[16];
                    snprintf(buf, sizeof(buf), "#%02x%02x%02x", fs.color.r, fs.color.g, fs.color.b);
                    fill = buf;
                }
                break;
            }
            if (p.lineStyle >= 0 && p.lineStyle < static_cast<i32>(shape.lineStyles.size())) {
                auto& ls = shape.lineStyles[p.lineStyle];
                char buf[16];
                snprintf(buf, sizeof(buf), "#%02x%02x%02x", ls.color.r, ls.color.g, ls.color.b);
                stroke = buf;
                strokeWidth = static_cast<f32>(ls.width) / 20.0f;
                break;
            }
        }

        svg << "  <path d=\"" << pathData << "\" fill=\"" << fill
            << "\" stroke=\"" << stroke << "\" stroke-width=\"" << strokeWidth << "\"/>\n";
    }

    svg << "</svg>\n";
    return svg.str();
}

bool SWFLoader::RasterizeShape(const SWFShape& shape, u32 width, u32 height,
                                std::vector<u8>& outPixels) {
    // Simple software rasterizer for shape preview
    outPixels.resize(width * height * 4, 0);

    // Compute bounding box
    f32 minX = static_cast<f32>(shape.bounds.xMin) / 20.0f;
    f32 maxX = static_cast<f32>(shape.bounds.xMax) / 20.0f;
    f32 minY = static_cast<f32>(shape.bounds.yMin) / 20.0f;
    f32 maxY = static_cast<f32>(shape.bounds.yMax) / 20.0f;

    f32 rangeX = maxX - minX;
    f32 rangeY = maxY - minY;
    if (rangeX < 0.001f || rangeY < 0.001f) return false;

    // Scale to fit
    f32 scaleX = static_cast<f32>(width) / rangeX;
    f32 scaleY = static_cast<f32>(height) / rangeY;
    f32 scale = (std::min)(scaleX, scaleY);

    // Draw edges as lines (simple wireframe preview)
    auto plotPixel = [&](i32 px, i32 py, u8 r, u8 g, u8 b) {
        if (px >= 0 && px < static_cast<i32>(width) && py >= 0 && py < static_cast<i32>(height)) {
            usize idx = (static_cast<usize>(py) * width + px) * 4;
            outPixels[idx] = r;
            outPixels[idx + 1] = g;
            outPixels[idx + 2] = b;
            outPixels[idx + 3] = 255;
        }
    };

    // Bresenham line
    auto drawLine = [&](f32 x0, f32 y0, f32 x1, f32 y1, u8 r, u8 g, u8 b) {
        i32 px0 = static_cast<i32>((x0 - minX) * scale);
        i32 py0 = static_cast<i32>((y0 - minY) * scale);
        i32 px1 = static_cast<i32>((x1 - minX) * scale);
        i32 py1 = static_cast<i32>((y1 - minY) * scale);

        i32 dx = std::abs(px1 - px0);
        i32 dy = -std::abs(py1 - py0);
        i32 sx = (px0 < px1) ? 1 : -1;
        i32 sy = (py0 < py1) ? 1 : -1;
        i32 err = dx + dy;

        for (i32 steps = 0; steps < 10000; ++steps) {
            plotPixel(px0, py0, r, g, b);
            if (px0 == px1 && py0 == py1) break;
            i32 e2 = 2 * err;
            if (e2 >= dy) { err += dy; px0 += sx; }
            if (e2 <= dx) { err += dx; py0 += sy; }
        }
    };

    for (auto& path : shape.paths) {
        u8 r = 200, g = 200, b = 200;
        if (path.fillStyle0 >= 0 && path.fillStyle0 < static_cast<i32>(shape.fillStyles.size())) {
            auto& fs = shape.fillStyles[path.fillStyle0];
            r = fs.color.r; g = fs.color.g; b = fs.color.b;
        }

        f32 cx = path.startX, cy = path.startY;
        for (auto& edge : path.edges) {
            if (edge.type == SWFShapeEdgeType::StraightLine) {
                drawLine(cx, cy, edge.ax, edge.ay, r, g, b);
            } else {
                // Approximate curve with line segments
                f32 px = cx, py = cy;
                for (i32 t = 1; t <= 8; ++t) {
                    f32 ft = static_cast<f32>(t) / 8.0f;
                    f32 mt = 1.0f - ft;
                    f32 nx = mt * mt * px + 2 * mt * ft * edge.cx + ft * ft * edge.ax;
                    f32 ny = mt * mt * py + 2 * mt * ft * edge.cy + ft * ft * edge.ay;
                    drawLine(px, py, nx, ny, r, g, b);
                    px = nx; py = ny;
                }
            }
            cx = edge.ax;
            cy = edge.ay;
        }
    }

    return true;
}

u32 SWFLoader::ExtractBitmaps(const SWFDocument& doc, const std::string& outputDir) {
    std::filesystem::create_directories(outputDir);
    u32 count = 0;

    for (auto& [id, bmp] : doc.bitmaps) {
        if (bmp.pixels.empty()) continue;
        std::string filename = outputDir + "/bitmap_" + std::to_string(id) + ".bin";
        std::ofstream out(filename, std::ios::binary);
        if (out.is_open()) {
            out.write(reinterpret_cast<const char*>(bmp.pixels.data()), bmp.pixels.size());
            count++;
        }
    }

    return count;
}

u32 SWFLoader::ExtractSounds(const SWFDocument& doc, const std::string& outputDir) {
    std::filesystem::create_directories(outputDir);
    u32 count = 0;

    for (auto& [id, snd] : doc.sounds) {
        if (snd.data.empty()) continue;
        std::string ext = (snd.format == "MP3") ? ".mp3" : ".bin";
        std::string filename = outputDir + "/sound_" + std::to_string(id) + ext;
        std::ofstream out(filename, std::ios::binary);
        if (out.is_open()) {
            out.write(reinterpret_cast<const char*>(snd.data.data()), snd.data.size());
            count++;
        }
    }

    return count;
}

std::string SWFLoader::GetInfo(const SWFDocument& doc) {
    std::ostringstream info;
    info << "SWF Version: " << static_cast<int>(doc.version) << "\n";
    info << "Compression: " << doc.compression << "\n";
    info << "Stage: " << doc.stageWidth << " x " << doc.stageHeight << "\n";
    info << "Frame Rate: " << doc.frameRate << " fps\n";
    info << "Frames: " << doc.mainTimeline.size() << "\n";
    info << "Shapes: " << doc.shapes.size() << "\n";
    info << "Sprites/MovieClips: " << doc.sprites.size() << "\n";
    info << "Bitmaps: " << doc.bitmaps.size() << "\n";
    info << "Sounds: " << doc.sounds.size() << "\n";
    info << "Texts: " << doc.texts.size() << "\n";
    info << "Tags: " << doc.tagCount << " (" << doc.unknownTagCount << " unknown)\n";

    if (!doc.exports.empty()) {
        info << "Exports:\n";
        for (auto& [id, name] : doc.exports) {
            info << "  " << id << " -> " << name << "\n";
        }
    }

    return info.str();
}

} // namespace Assets
} // namespace Enjin
