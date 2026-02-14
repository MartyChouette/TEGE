#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Assets/SWFLoader.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/Animation/Timeline.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace Enjin {

namespace Assets {

// ============================================================================
// SWF Import Options
// ============================================================================

struct SWFImportOptions {
    bool rasterizeShapes = true;        // Rasterize vector shapes to sprite PNGs
    f32 rasterScale = 2.0f;            // Rasterization DPI multiplier (1x, 2x, 4x)
    bool importSounds = true;           // Extract and import sounds
    bool importTimelines = true;        // Create TimelineComponents for MovieClips
    std::string outputDirectory;        // Where to write extracted asset files
};

// ============================================================================
// SWF Converter
// ============================================================================
// Converts a parsed SWFDocument into Enjin ECS entities:
//   - Shapes are rasterized to PNG and become Sprite2D entities
//   - Bitmaps are extracted and become Sprite2D entities
//   - MovieClips (Sprites) become parent entities with child entities per depth
//     and TimelineComponents for keyframe animation
//   - Texts become TextComponent entities
//   - Stage dimensions configure the camera ortho size
//   - SWF coordinate system (twips, Y-down) is converted to Enjin (pixels, Y-up for 2D)

class ENJIN_API SWFConverter {
public:
    // Convert a parsed SWFDocument into ECS entities
    // outputDir: directory where rasterized shapes, bitmaps, sounds are written
    static SWFConversionResult ConvertToEntities(
        const SWFDocument& doc,
        ECS::World* world,
        const SWFImportOptions& options = {});

private:
    // Convert a single SWFShape to an entity with Sprite2DComponent
    static ECS::Entity ConvertShape(
        const SWFShape& shape,
        u16 characterId,
        ECS::World* world,
        const SWFImportOptions& options,
        SWFConversionResult& result);

    // Convert a single SWFBitmap to an entity with Sprite2DComponent
    static ECS::Entity ConvertBitmap(
        const SWFBitmap& bitmap,
        u16 characterId,
        ECS::World* world,
        const SWFImportOptions& options,
        SWFConversionResult& result);

    // Convert a single SWFText to an entity with TextComponent
    static ECS::Entity ConvertText(
        const SWFText& text,
        u16 characterId,
        ECS::World* world,
        const SWFDocument& doc,
        SWFConversionResult& result);

    // Convert a SWFSprite (MovieClip) to a parent entity with child entities
    // and TimelineComponent for animation
    static ECS::Entity ConvertMovieClip(
        const SWFSprite& sprite,
        u16 characterId,
        ECS::World* world,
        const SWFDocument& doc,
        const SWFImportOptions& options,
        const std::unordered_map<u16, ECS::Entity>& characterEntityMap,
        SWFConversionResult& result);

    // Apply SWFMatrix to a TransformComponent
    // Converts SWF coordinate system (Y-down) to Enjin 2D (Y-up)
    static void ApplyMatrix(
        const SWFMatrix& matrix,
        ECS::TransformComponent& transform,
        f32 stageHeight);

    // Apply SWFColorTransform to MaterialComponent tint
    static void ApplyColorTransform(
        const SWFColorTransform& ct,
        ECS::MaterialComponent& material);

    // Create a camera entity configured for the SWF stage dimensions
    static ECS::Entity CreateStageCamera(
        const SWFDocument& doc,
        ECS::World* world);

    // Save RGBA pixel data to a PNG file
    static bool SavePNG(
        const std::string& path,
        const u8* pixels,
        u32 width,
        u32 height);

    // Build timeline keyframes from SWFFrame data
    static void BuildTimeline(
        const std::vector<SWFFrame>& frames,
        Animation::TimelineComponent& timeline,
        const std::unordered_map<u16, ECS::Entity>& depthEntityMap,
        f32 frameRate,
        f32 stageHeight);
};

} // namespace Assets
} // namespace Enjin
