#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

namespace Enjin {
namespace Assets {

// ============================================================================
// SWF Binary Format Parser
// ============================================================================
// Parses Adobe SWF (Small Web Format) files and extracts:
// - Shape definitions (paths, fills, strokes → SVG or rasterized sprites)
// - Sprite/MovieClip definitions (symbol library)
// - Timeline data (frames, keyframes, tweens, labels)
// - ActionScript bytecode tags (for transpilation)
//
// Reference: SWF File Format Specification (Adobe, version 19+)

// --- Low-level SWF structures ---

struct SWFRect {
    i32 xMin = 0, xMax = 0, yMin = 0, yMax = 0;  // In twips (1/20 pixel)
};

struct SWFColor {
    u8 r = 0, g = 0, b = 0, a = 255;
};

struct SWFMatrix {
    f32 scaleX = 1.0f, scaleY = 1.0f;
    f32 rotateSkew0 = 0.0f, rotateSkew1 = 0.0f;
    f32 translateX = 0.0f, translateY = 0.0f;  // In pixels (converted from twips)
};

struct SWFColorTransform {
    f32 redMultTerm = 1.0f, greenMultTerm = 1.0f, blueMultTerm = 1.0f, alphaMultTerm = 1.0f;
    i32 redAddTerm = 0, greenAddTerm = 0, blueAddTerm = 0, alphaAddTerm = 0;
};

// --- Shape data ---

enum class SWFShapeEdgeType : u8 {
    StraightLine,
    CurvedLine   // Quadratic Bezier
};

struct SWFShapeEdge {
    SWFShapeEdgeType type = SWFShapeEdgeType::StraightLine;
    f32 cx = 0, cy = 0;       // Control point (for curves)
    f32 ax = 0, ay = 0;       // Anchor/end point
};

struct SWFShapePath {
    f32 startX = 0, startY = 0;
    std::vector<SWFShapeEdge> edges;
    i32 fillStyle0 = -1;       // Index into fill styles (-1 = none)
    i32 fillStyle1 = -1;
    i32 lineStyle = -1;
};

enum class SWFFillType : u8 {
    Solid,
    LinearGradient,
    RadialGradient,
    Bitmap
};

struct SWFFillStyle {
    SWFFillType type = SWFFillType::Solid;
    SWFColor color;
    // Gradient stops (for gradient fills)
    std::vector<std::pair<u8, SWFColor>> gradientStops;  // ratio (0-255), color
    u16 bitmapId = 0;
    SWFMatrix bitmapMatrix;
};

struct SWFLineStyle {
    u16 width = 20;  // In twips
    SWFColor color;
};

struct SWFShape {
    u16 characterId = 0;
    SWFRect bounds;
    std::vector<SWFFillStyle> fillStyles;
    std::vector<SWFLineStyle> lineStyles;
    std::vector<SWFShapePath> paths;
};

// --- Bitmap/Image data ---

struct SWFBitmap {
    u16 characterId = 0;
    u32 width = 0, height = 0;
    std::vector<u8> pixels;  // RGBA8
    std::string format;      // "JPEG", "PNG", "Lossless"
};

// --- Text data ---

struct SWFText {
    u16 characterId = 0;
    std::string text;
    std::string fontName;
    f32 fontSize = 12.0f;
    SWFColor color;
    SWFRect bounds;
};

// --- Sound data ---

struct SWFSound {
    u16 characterId = 0;
    u32 sampleRate = 44100;
    u8 channels = 1;
    u8 bitsPerSample = 16;
    std::string format;     // "MP3", "ADPCM", "Raw", "Nellymoser"
    std::vector<u8> data;
};

// --- Timeline / Display list ---

struct SWFPlaceObject {
    u16 depth = 0;
    u16 characterId = 0;
    bool hasCharacter = false;
    bool hasMatrix = false;
    bool hasColorTransform = false;
    bool hasName = false;
    bool move = false;        // true = modify existing at depth
    SWFMatrix matrix;
    SWFColorTransform colorTransform;
    std::string name;         // Instance name (for AS access)
    u16 clipDepth = 0;        // Mask layer
    u16 ratio = 0;            // Morph shape ratio
};

struct SWFRemoveObject {
    u16 depth = 0;
};

struct SWFFrameLabel {
    std::string name;
};

struct SWFFrame {
    u32 frameIndex = 0;
    std::vector<SWFPlaceObject> placements;
    std::vector<SWFRemoveObject> removals;
    SWFFrameLabel label;
    bool hasLabel = false;
    std::vector<u8> actionData;  // Raw AS bytecode (for transpilation)
    bool hasAction = false;
};

// --- Sprite / MovieClip ---

struct SWFSprite {
    u16 characterId = 0;
    u16 frameCount = 0;
    std::vector<SWFFrame> frames;
    std::string exportName;   // linkage class name
};

// --- Top-level SWF document ---

struct SWFDocument {
    // Header
    u8 version = 0;
    u32 fileLength = 0;
    SWFRect frameSize;
    f32 frameRate = 24.0f;
    u16 frameCount = 0;
    bool compressed = false;    // zlib or LZMA
    std::string compression;    // "none", "zlib", "lzma"

    // Character dictionary
    std::unordered_map<u16, SWFShape> shapes;
    std::unordered_map<u16, SWFBitmap> bitmaps;
    std::unordered_map<u16, SWFText> texts;
    std::unordered_map<u16, SWFSound> sounds;
    std::unordered_map<u16, SWFSprite> sprites;

    // Main timeline
    std::vector<SWFFrame> mainTimeline;

    // Export table (symbol class / export assets)
    std::unordered_map<u16, std::string> exports;  // characterId → class name

    // Background color
    SWFColor backgroundColor;

    // Stage dimensions in pixels
    f32 stageWidth = 0;
    f32 stageHeight = 0;

    // Stats
    u32 tagCount = 0;
    u32 unknownTagCount = 0;
    std::vector<std::string> warnings;
};

// --- Parse result ---

struct SWFParseResult {
    bool success = false;
    std::string errorMessage;
    SWFDocument document;
};

// --- Conversion result ---

struct SWFConversionResult {
    bool success = false;
    std::string errorMessage;
    u32 entitiesCreated = 0;
    u32 spritesConverted = 0;
    u32 shapesRasterized = 0;
    u32 soundsExtracted = 0;
    std::vector<std::string> warnings;
    std::vector<std::string> generatedFiles;  // Paths to exported sprites/sounds
};

// ============================================================================
// SWF Loader
// ============================================================================

class ENJIN_API SWFLoader {
public:
    // Parse a .swf file into a SWFDocument
    static SWFParseResult Parse(const std::string& filepath);

    // Parse from memory buffer
    static SWFParseResult ParseFromMemory(const u8* data, usize size);

    // Convert a parsed SWF to an SVG string (shapes only, for vector preview)
    static std::string ConvertToSVG(const SWFDocument& doc);

    // Convert a SWF shape to an SVG path string
    static std::string ShapeToSVGPath(const SWFShape& shape);

    // Rasterize a shape to RGBA pixels at given resolution
    static bool RasterizeShape(const SWFShape& shape, u32 width, u32 height,
                               std::vector<u8>& outPixels);

    // Extract all bitmaps as PNG files to a directory
    static u32 ExtractBitmaps(const SWFDocument& doc, const std::string& outputDir);

    // Extract all sounds to a directory
    static u32 ExtractSounds(const SWFDocument& doc, const std::string& outputDir);

    // Get human-readable SWF info string
    static std::string GetInfo(const SWFDocument& doc);

private:
    // Binary reader helper
    struct BitReader {
        const u8* data = nullptr;
        usize size = 0;
        usize bytePos = 0;
        u8 bitPos = 0;

        u32 ReadBits(u8 count);
        i32 ReadSignedBits(u8 count);
        u8 ReadU8();
        u16 ReadU16();
        u32 ReadU32();
        i16 ReadI16();
        i32 ReadI32();
        f32 ReadFixed8();
        f32 ReadFixed16();
        std::string ReadString();
        void AlignByte();
        bool HasMore() const;
        void Skip(usize bytes);
    };

    // Tag parsing
    struct TagHeader {
        u16 type = 0;
        u32 length = 0;
    };

    static TagHeader ReadTagHeader(BitReader& reader);
    static SWFRect ReadRect(BitReader& reader);
    static SWFMatrix ReadMatrix(BitReader& reader);
    static SWFColorTransform ReadColorTransform(BitReader& reader, bool hasAlpha);
    static SWFColor ReadRGB(BitReader& reader);
    static SWFColor ReadRGBA(BitReader& reader);

    // Shape parsing
    static SWFShape ParseDefineShape(BitReader& reader, u8 shapeVersion);
    static std::vector<SWFFillStyle> ReadFillStyles(BitReader& reader, u8 shapeVersion);
    static std::vector<SWFLineStyle> ReadLineStyles(BitReader& reader, u8 shapeVersion);
    static std::vector<SWFShapePath> ReadShapeRecords(BitReader& reader, u8 shapeVersion,
                                                       i32 numFillBits, i32 numLineBits);

    // Other tag parsers
    static SWFBitmap ParseDefineBitsJPEG(BitReader& reader, u8 jpegVersion, u32 tagLength);
    static SWFBitmap ParseDefineBitsLossless(BitReader& reader, u8 losslessVersion, u32 tagLength);
    static SWFSprite ParseDefineSprite(BitReader& reader);
    static SWFPlaceObject ParsePlaceObject2(BitReader& reader, u8 version);
    static SWFText ParseDefineText(BitReader& reader, u8 textVersion);
    static SWFSound ParseDefineSound(BitReader& reader, u32 tagLength);

    // Decompression
    static bool DecompressZlib(const u8* input, usize inputSize,
                               std::vector<u8>& output, usize expectedSize);
};

} // namespace Assets
} // namespace Enjin
