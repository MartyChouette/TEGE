#include "Enjin/Assets/SWFConverter.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Camera.h"
#include "Enjin/ECS/Components/Text.h"
#include "Enjin/ECS/Components/Hierarchy.h"
#include "Enjin/Animation/Timeline.h"
#include "Enjin/Math/Quaternion.h"
#include "Enjin/Logging/Log.h"

#include <filesystem>
#include <fstream>
#include <cmath>
#include <algorithm>

// stb_image_write for PNG export (declared as extern if available)
#ifndef STB_IMAGE_WRITE_DECLARED
extern "C" {
    int stbi_write_png(const char* filename, int w, int h, int comp,
                       const void* data, int stride_in_bytes);
}
#define STB_IMAGE_WRITE_DECLARED
#endif

namespace Enjin {
namespace Assets {

// ============================================================================
// PNG save helper
// ============================================================================

bool SWFConverter::SavePNG(const std::string& path, const u8* pixels,
                            u32 width, u32 height) {
    if (!pixels || width == 0 || height == 0) return false;
    i32 result = stbi_write_png(path.c_str(),
                                static_cast<i32>(width),
                                static_cast<i32>(height),
                                4, pixels, static_cast<i32>(width * 4));
    return result != 0;
}

// ============================================================================
// Coordinate conversion helpers
// ============================================================================

void SWFConverter::ApplyMatrix(const SWFMatrix& matrix,
                                ECS::TransformComponent& transform,
                                f32 stageHeight) {
    // SWF translateX/Y are already in pixels (converted from twips by the parser).
    // SWF uses Y-down; Enjin 2D uses Y-up, so flip Y relative to stage height.
    transform.position.x = matrix.translateX;
    transform.position.y = stageHeight - matrix.translateY;
    transform.position.z = 0.0f;

    // Scale from SWF matrix
    transform.scale.x = matrix.scaleX;
    transform.scale.y = matrix.scaleY;
    transform.scale.z = 1.0f;

    // Rotation: SWF uses a 2D affine matrix with rotateSkew0/1.
    // For pure rotation: rotateSkew0 = sin(angle), rotateSkew1 = -sin(angle)
    // Extract the angle and apply as Z rotation (negated for Y-flip).
    f32 angle = std::atan2(matrix.rotateSkew0, matrix.scaleX);
    transform.rotation = Math::Quaternion(Math::Vector3(0.0f, 0.0f, 1.0f), -angle);
}

void SWFConverter::ApplyColorTransform(const SWFColorTransform& ct,
                                        ECS::MaterialComponent& material) {
    // Apply multiplicative color terms as base color tint
    material.baseColor.x = std::clamp(ct.redMultTerm + static_cast<f32>(ct.redAddTerm) / 255.0f, 0.0f, 1.0f);
    material.baseColor.y = std::clamp(ct.greenMultTerm + static_cast<f32>(ct.greenAddTerm) / 255.0f, 0.0f, 1.0f);
    material.baseColor.z = std::clamp(ct.blueMultTerm + static_cast<f32>(ct.blueAddTerm) / 255.0f, 0.0f, 1.0f);
    material.opacity = std::clamp(ct.alphaMultTerm + static_cast<f32>(ct.alphaAddTerm) / 255.0f, 0.0f, 1.0f);
}

// ============================================================================
// Shape conversion
// ============================================================================

ECS::Entity SWFConverter::ConvertShape(const SWFShape& shape,
                                        u16 characterId,
                                        ECS::World* world,
                                        const SWFImportOptions& options,
                                        SWFConversionResult& result) {
    ECS::Entity entity = world->CreateEntity();

    // Name
    std::string name = "SWF_Shape_" + std::to_string(characterId);
    world->AddComponent<ECS::NameComponent>(entity, ECS::NameComponent(name));

    // Transform
    auto& tf = world->AddComponent<ECS::TransformComponent>(entity);
    tf.position = Math::Vector3(0.0f);
    tf.scale = Math::Vector3(1.0f);

    // Rasterize shape to PNG if requested
    if (options.rasterizeShapes && !options.outputDirectory.empty()) {
        // Compute rasterization dimensions from shape bounds
        f32 boundsW = static_cast<f32>(shape.bounds.xMax - shape.bounds.xMin) / 20.0f;
        f32 boundsH = static_cast<f32>(shape.bounds.yMax - shape.bounds.yMin) / 20.0f;
        if (boundsW < 1.0f) boundsW = 1.0f;
        if (boundsH < 1.0f) boundsH = 1.0f;

        u32 rasterW = static_cast<u32>(boundsW * options.rasterScale);
        u32 rasterH = static_cast<u32>(boundsH * options.rasterScale);
        // Clamp to reasonable size
        rasterW = std::clamp(rasterW, 1u, 4096u);
        rasterH = std::clamp(rasterH, 1u, 4096u);

        std::vector<u8> pixels;
        if (SWFLoader::RasterizeShape(shape, rasterW, rasterH, pixels)) {
            std::filesystem::create_directories(options.outputDirectory);
            std::string pngPath = options.outputDirectory + "/shape_" +
                                  std::to_string(characterId) + ".png";
            if (SavePNG(pngPath, pixels.data(), rasterW, rasterH)) {
                // Add Sprite2DComponent with the rasterized texture
                auto& sprite = world->AddComponent<ECS::Sprite2DComponent>(entity);
                sprite.texturePath = pngPath;
                sprite.size = Math::Vector2(boundsW / 100.0f, boundsH / 100.0f);

                result.generatedFiles.push_back(pngPath);
                result.shapesRasterized++;

                ENJIN_LOG_INFO(Assets, "SWF shape %u rasterized: %ux%u -> %s",
                               characterId, rasterW, rasterH, pngPath.c_str());
            } else {
                result.warnings.push_back("Failed to save rasterized shape " +
                                          std::to_string(characterId));
            }
        } else {
            result.warnings.push_back("Failed to rasterize shape " +
                                      std::to_string(characterId));
        }
    }

    // Material with default white tint (will be overridden by color transform if placed)
    auto& mat = world->AddComponent<ECS::MaterialComponent>(entity);
    mat.baseColor = Math::Vector3(1.0f, 1.0f, 1.0f);

    // Apply fill color from the first solid fill style as tint
    for (auto& fs : shape.fillStyles) {
        if (fs.type == SWFFillType::Solid) {
            mat.baseColor.x = static_cast<f32>(fs.color.r) / 255.0f;
            mat.baseColor.y = static_cast<f32>(fs.color.g) / 255.0f;
            mat.baseColor.z = static_cast<f32>(fs.color.b) / 255.0f;
            mat.opacity = static_cast<f32>(fs.color.a) / 255.0f;
            break;
        }
    }

    result.entitiesCreated++;
    return entity;
}

// ============================================================================
// Bitmap conversion
// ============================================================================

ECS::Entity SWFConverter::ConvertBitmap(const SWFBitmap& bitmap,
                                         u16 characterId,
                                         ECS::World* world,
                                         const SWFImportOptions& options,
                                         SWFConversionResult& result) {
    ECS::Entity entity = world->CreateEntity();

    std::string name = "SWF_Bitmap_" + std::to_string(characterId);
    world->AddComponent<ECS::NameComponent>(entity, ECS::NameComponent(name));

    auto& tf = world->AddComponent<ECS::TransformComponent>(entity);
    tf.position = Math::Vector3(0.0f);
    tf.scale = Math::Vector3(1.0f);

    // Extract bitmap to file
    if (!options.outputDirectory.empty() && !bitmap.pixels.empty()) {
        std::filesystem::create_directories(options.outputDirectory);

        std::string ext = ".bin";
        if (bitmap.format == "JPEG") ext = ".jpg";
        else if (bitmap.format == "PNG") ext = ".png";

        std::string filePath = options.outputDirectory + "/bitmap_" +
                               std::to_string(characterId) + ext;

        std::ofstream out(filePath, std::ios::binary);
        if (out.is_open()) {
            out.write(reinterpret_cast<const char*>(bitmap.pixels.data()),
                      bitmap.pixels.size());
            out.close();

            // Only add Sprite2DComponent when we have a valid texture path
            auto& sprite = world->AddComponent<ECS::Sprite2DComponent>(entity);
            sprite.texturePath = filePath;
            if (bitmap.width > 0 && bitmap.height > 0) {
                sprite.size = Math::Vector2(
                    static_cast<f32>(bitmap.width) / 100.0f,
                    static_cast<f32>(bitmap.height) / 100.0f);
            }

            result.generatedFiles.push_back(filePath);

            ENJIN_LOG_INFO(Assets, "SWF bitmap %u extracted: %ux%u %s -> %s",
                           characterId, bitmap.width, bitmap.height,
                           bitmap.format.c_str(), filePath.c_str());
        } else {
            result.warnings.push_back("Failed to extract bitmap " +
                                      std::to_string(characterId));
        }
    }

    // Material
    auto& mat = world->AddComponent<ECS::MaterialComponent>(entity);
    mat.baseColor = Math::Vector3(1.0f, 1.0f, 1.0f);

    result.entitiesCreated++;
    return entity;
}

// ============================================================================
// Text conversion
// ============================================================================

ECS::Entity SWFConverter::ConvertText(const SWFText& text,
                                       u16 characterId,
                                       ECS::World* world,
                                       const SWFDocument& doc,
                                       SWFConversionResult& result) {
    ECS::Entity entity = world->CreateEntity();

    std::string name = "SWF_Text_" + std::to_string(characterId);
    world->AddComponent<ECS::NameComponent>(entity, ECS::NameComponent(name));

    // Transform: position from text bounds, Y-flipped
    auto& tf = world->AddComponent<ECS::TransformComponent>(entity);
    f32 textX = static_cast<f32>(text.bounds.xMin) / 20.0f;
    f32 textY = static_cast<f32>(text.bounds.yMin) / 20.0f;
    tf.position.x = textX;
    tf.position.y = doc.stageHeight - textY;
    tf.position.z = 0.0f;
    tf.scale = Math::Vector3(1.0f);

    // TextComponent
    auto& tc = world->AddComponent<ECS::TextComponent>(entity);
    tc.text = text.text;
    tc.fontSize = text.fontSize;
    tc.textColor = Math::Vector3(
        static_cast<f32>(text.color.r) / 255.0f,
        static_cast<f32>(text.color.g) / 255.0f,
        static_cast<f32>(text.color.b) / 255.0f);
    if (!text.fontName.empty()) {
        tc.fontPath = text.fontName;
    }

    // Material
    auto& mat = world->AddComponent<ECS::MaterialComponent>(entity);
    mat.baseColor = Math::Vector3(1.0f, 1.0f, 1.0f);

    result.entitiesCreated++;
    return entity;
}

// ============================================================================
// MovieClip (Sprite) conversion
// ============================================================================

void SWFConverter::BuildTimeline(const std::vector<SWFFrame>& frames,
                                  Animation::TimelineComponent& timeline,
                                  const std::unordered_map<u16, ECS::Entity>& depthEntityMap,
                                  f32 frameRate,
                                  f32 stageHeight) {
    if (frames.empty() || frameRate <= 0.0f) return;

    f32 frameDuration = 1.0f / frameRate;
    timeline.duration = static_cast<f32>(frames.size()) * frameDuration;
    timeline.loop = true;
    timeline.playOnAwake = true;

    // Build property tracks from placement data.
    // Each depth that gets a PlaceObject generates position/scale/rotation keyframes.
    // We track the most recent state per depth across frames.
    struct DepthState {
        u16 characterId = 0;
        SWFMatrix matrix;
        SWFColorTransform colorTransform;
        bool active = false;
    };
    std::unordered_map<u16, DepthState> depthStates;

    for (auto& frame : frames) {
        f32 time = static_cast<f32>(frame.frameIndex) * frameDuration;

        // Process removals first
        for (auto& removal : frame.removals) {
            auto it = depthStates.find(removal.depth);
            if (it != depthStates.end()) {
                it->second.active = false;

                // Add visibility keyframe (hide)
                auto entityIt = depthEntityMap.find(removal.depth);
                if (entityIt != depthEntityMap.end()) {
                    Animation::PropertyTrack visTrack;
                    visTrack.targetEntity = entityIt->second;
                    visTrack.targetProperty = "visible";
                    Animation::PropertyKeyframe kf;
                    kf.time = time;
                    kf.value = false;
                    visTrack.keyframes.push_back(kf);
                    timeline.propertyTracks.push_back(std::move(visTrack));
                }
            }
        }

        // Process placements
        for (auto& placement : frame.placements) {
            auto& state = depthStates[placement.depth];

            if (placement.hasCharacter) {
                state.characterId = placement.characterId;
                state.active = true;
            }
            if (placement.hasMatrix) {
                state.matrix = placement.matrix;
            }
            if (placement.hasColorTransform) {
                state.colorTransform = placement.colorTransform;
            }

            // Find the entity for this depth
            auto entityIt = depthEntityMap.find(placement.depth);
            if (entityIt == depthEntityMap.end()) continue;
            ECS::Entity targetEntity = entityIt->second;

            // Position keyframe
            {
                Animation::PropertyTrack posTrack;
                posTrack.targetEntity = targetEntity;
                posTrack.targetProperty = "position";
                Animation::PropertyKeyframe kf;
                kf.time = time;
                kf.easing = Animation::TimelineEasing::Linear;
                kf.value = Math::Vector3(
                    state.matrix.translateX,
                    stageHeight - state.matrix.translateY,
                    0.0f);
                posTrack.keyframes.push_back(kf);
                timeline.propertyTracks.push_back(std::move(posTrack));
            }

            // Scale keyframe (only if non-default)
            if (std::abs(state.matrix.scaleX - 1.0f) > 0.001f ||
                std::abs(state.matrix.scaleY - 1.0f) > 0.001f) {
                Animation::PropertyTrack scaleTrack;
                scaleTrack.targetEntity = targetEntity;
                scaleTrack.targetProperty = "scale";
                Animation::PropertyKeyframe kf;
                kf.time = time;
                kf.value = Math::Vector3(state.matrix.scaleX, state.matrix.scaleY, 1.0f);
                scaleTrack.keyframes.push_back(kf);
                timeline.propertyTracks.push_back(std::move(scaleTrack));
            }

            // Rotation keyframe (only if rotation is present)
            if (std::abs(state.matrix.rotateSkew0) > 0.001f ||
                std::abs(state.matrix.rotateSkew1) > 0.001f) {
                Animation::PropertyTrack rotTrack;
                rotTrack.targetEntity = targetEntity;
                rotTrack.targetProperty = "rotation.z";
                Animation::PropertyKeyframe kf;
                kf.time = time;
                // Extract Z angle in degrees from the SWF matrix, negated for Y-flip
                f32 angle = std::atan2(state.matrix.rotateSkew0, state.matrix.scaleX);
                kf.value = -angle * 180.0f / 3.14159265f;
                rotTrack.keyframes.push_back(kf);
                timeline.propertyTracks.push_back(std::move(rotTrack));
            }

            // Opacity keyframe from color transform
            if (state.colorTransform.alphaMultTerm < 0.999f ||
                state.colorTransform.alphaAddTerm != 0) {
                Animation::PropertyTrack alphaTrack;
                alphaTrack.targetEntity = targetEntity;
                alphaTrack.targetProperty = "material.opacity";
                Animation::PropertyKeyframe kf;
                kf.time = time;
                kf.value = std::clamp(
                    state.colorTransform.alphaMultTerm +
                    static_cast<f32>(state.colorTransform.alphaAddTerm) / 255.0f,
                    0.0f, 1.0f);
                alphaTrack.keyframes.push_back(kf);
                timeline.propertyTracks.push_back(std::move(alphaTrack));
            }
        }

        // Frame labels become timeline events (markers)
        if (frame.hasLabel && !frame.label.name.empty()) {
            if (timeline.eventTracks.empty()) {
                Animation::EventTrack et;
                et.name = "FrameLabels";
                timeline.eventTracks.push_back(std::move(et));
            }
            Animation::TimelineEvent evt;
            evt.time = time;
            evt.eventName = "frameLabel";
            evt.eventData = frame.label.name;
            timeline.eventTracks[0].events.push_back(evt);
        }
    }
}

ECS::Entity SWFConverter::ConvertMovieClip(
    const SWFSprite& sprite,
    u16 characterId,
    ECS::World* world,
    const SWFDocument& doc,
    const SWFImportOptions& options,
    const std::unordered_map<u16, ECS::Entity>& characterEntityMap,
    SWFConversionResult& result) {

    // Create parent entity for the MovieClip
    ECS::Entity parentEntity = world->CreateEntity();

    std::string name = sprite.exportName.empty()
        ? ("SWF_MovieClip_" + std::to_string(characterId))
        : sprite.exportName;
    world->AddComponent<ECS::NameComponent>(parentEntity, ECS::NameComponent(name));

    auto& tf = world->AddComponent<ECS::TransformComponent>(parentEntity);
    tf.position = Math::Vector3(0.0f);
    tf.scale = Math::Vector3(1.0f);

    result.entitiesCreated++;
    result.spritesConverted++;

    // Scan all frames to determine which depths are used and what characters are placed there.
    // Create a child entity for each unique depth.
    std::unordered_map<u16, u16> depthToCharacter;  // depth -> characterId
    std::unordered_map<u16, std::string> depthToName;

    for (auto& frame : sprite.frames) {
        for (auto& placement : frame.placements) {
            if (placement.hasCharacter) {
                depthToCharacter[placement.depth] = placement.characterId;
            }
            if (placement.hasName && !placement.name.empty()) {
                depthToName[placement.depth] = placement.name;
            }
        }
    }

    // Create child entities for each depth
    std::unordered_map<u16, ECS::Entity> depthEntityMap;
    for (auto& [depth, charId] : depthToCharacter) {
        ECS::Entity childEntity = world->CreateEntity();

        // Name from instance name or fallback to depth + character
        std::string childName;
        auto nameIt = depthToName.find(depth);
        if (nameIt != depthToName.end()) {
            childName = nameIt->second;
        } else {
            childName = name + "_depth_" + std::to_string(depth);
        }
        world->AddComponent<ECS::NameComponent>(childEntity, ECS::NameComponent(childName));

        // Transform
        auto& childTf = world->AddComponent<ECS::TransformComponent>(childEntity);
        childTf.position = Math::Vector3(0.0f);
        childTf.scale = Math::Vector3(1.0f);

        // If the character has a corresponding entity with a Sprite2DComponent,
        // clone its texture path to this child entity.
        auto charIt = characterEntityMap.find(charId);
        if (charIt != characterEntityMap.end()) {
            auto* srcSprite = world->GetComponent<ECS::Sprite2DComponent>(charIt->second);
            if (srcSprite && !srcSprite->texturePath.empty()) {
                auto& childSprite = world->AddComponent<ECS::Sprite2DComponent>(childEntity);
                childSprite.texturePath = srcSprite->texturePath;
                childSprite.size = srcSprite->size;
            }
        }

        // Material
        auto& childMat = world->AddComponent<ECS::MaterialComponent>(childEntity);
        childMat.baseColor = Math::Vector3(1.0f, 1.0f, 1.0f);

        // Sort order by depth
        auto* childSprite = world->GetComponent<ECS::Sprite2DComponent>(childEntity);
        if (childSprite) {
            childSprite->orderInLayer = static_cast<i32>(depth);
        }

        // Parent-child hierarchy
        ECS::SetParent(world, childEntity, parentEntity);

        depthEntityMap[depth] = childEntity;
        result.entitiesCreated++;
    }

    // Build timeline from frames
    if (options.importTimelines && !sprite.frames.empty()) {
        auto& timeline = world->AddComponent<Animation::TimelineComponent>(parentEntity);
        BuildTimeline(sprite.frames, timeline, depthEntityMap,
                      doc.frameRate, doc.stageHeight);

        ENJIN_LOG_INFO(Assets, "SWF MovieClip '%s': %u frames, %u depths, %.1fs duration",
                       name.c_str(),
                       static_cast<u32>(sprite.frames.size()),
                       static_cast<u32>(depthEntityMap.size()),
                       timeline.duration);
    }

    return parentEntity;
}

// ============================================================================
// Camera creation
// ============================================================================

ECS::Entity SWFConverter::CreateStageCamera(const SWFDocument& doc,
                                             ECS::World* world) {
    ECS::Entity camEntity = world->CreateEntity();

    world->AddComponent<ECS::NameComponent>(camEntity, ECS::NameComponent("SWF_Camera"));

    auto& tf = world->AddComponent<ECS::TransformComponent>(camEntity);
    // Center the camera on the stage
    tf.position = Math::Vector3(doc.stageWidth * 0.5f, doc.stageHeight * 0.5f, 10.0f);
    tf.scale = Math::Vector3(1.0f);

    auto& cam = world->AddComponent<ECS::CameraComponent>(camEntity);
    cam.projectionType = ECS::ProjectionType::Orthographic;
    // orthoSize is half-height of the view
    cam.orthoSize = doc.stageHeight * 0.5f / 100.0f;  // Convert to world units
    cam.isActive = true;
    cam.priority = 10;
    cam.backgroundColor = Math::Vector3(
        static_cast<f32>(doc.backgroundColor.r) / 255.0f,
        static_cast<f32>(doc.backgroundColor.g) / 255.0f,
        static_cast<f32>(doc.backgroundColor.b) / 255.0f);

    return camEntity;
}

// ============================================================================
// Main conversion function
// ============================================================================

SWFConversionResult SWFConverter::ConvertToEntities(
    const SWFDocument& doc,
    ECS::World* world,
    const SWFImportOptions& options) {

    SWFConversionResult result;
    result.success = false;

    if (!world) {
        result.errorMessage = "World pointer is null";
        return result;
    }

    ENJIN_LOG_INFO(Assets, "SWF conversion starting: %.0fx%.0f, %u shapes, %u bitmaps, "
                           "%u sprites, %u texts, %u sounds",
                   doc.stageWidth, doc.stageHeight,
                   static_cast<u32>(doc.shapes.size()),
                   static_cast<u32>(doc.bitmaps.size()),
                   static_cast<u32>(doc.sprites.size()),
                   static_cast<u32>(doc.texts.size()),
                   static_cast<u32>(doc.sounds.size()));

    // Ensure output directory exists
    if (!options.outputDirectory.empty()) {
        std::filesystem::create_directories(options.outputDirectory);
    }

    // Map characterId -> entity for cross-referencing (shapes placed in MovieClips, etc.)
    std::unordered_map<u16, ECS::Entity> characterEntityMap;

    // 1. Convert shapes
    for (auto& [id, shape] : doc.shapes) {
        ECS::Entity entity = ConvertShape(shape, id, world, options, result);
        characterEntityMap[id] = entity;
    }

    // 2. Convert bitmaps
    for (auto& [id, bitmap] : doc.bitmaps) {
        ECS::Entity entity = ConvertBitmap(bitmap, id, world, options, result);
        characterEntityMap[id] = entity;
    }

    // 3. Convert texts
    for (auto& [id, text] : doc.texts) {
        ECS::Entity entity = ConvertText(text, id, world, doc, result);
        characterEntityMap[id] = entity;
    }

    // 4. Convert MovieClips (sprites)
    for (auto& [id, sprite] : doc.sprites) {
        ECS::Entity entity = ConvertMovieClip(sprite, id, world, doc, options,
                                              characterEntityMap, result);
        characterEntityMap[id] = entity;
    }

    // 5. Process main timeline placements (position entities on stage)
    for (auto& frame : doc.mainTimeline) {
        for (auto& placement : frame.placements) {
            if (!placement.hasCharacter) continue;

            auto it = characterEntityMap.find(placement.characterId);
            if (it == characterEntityMap.end()) continue;

            ECS::Entity entity = it->second;

            // Apply transform
            if (placement.hasMatrix) {
                auto* tf = world->GetComponent<ECS::TransformComponent>(entity);
                if (tf) {
                    ApplyMatrix(placement.matrix, *tf, doc.stageHeight);
                }
            }

            // Apply color transform
            if (placement.hasColorTransform) {
                auto* mat = world->GetComponent<ECS::MaterialComponent>(entity);
                if (mat) {
                    ApplyColorTransform(placement.colorTransform, *mat);
                }
            }

            // Apply instance name
            if (placement.hasName && !placement.name.empty()) {
                world->SetEntityName(entity, placement.name);
            }
        }
    }

    // 6. Extract sounds
    if (options.importSounds && !options.outputDirectory.empty()) {
        u32 soundCount = SWFLoader::ExtractSounds(doc, options.outputDirectory);
        result.soundsExtracted = soundCount;
        if (soundCount > 0) {
            // Add generated sound file paths to result
            for (auto& [id, snd] : doc.sounds) {
                if (snd.data.empty()) continue;
                std::string ext = (snd.format == "MP3") ? ".mp3" : ".bin";
                std::string soundPath = options.outputDirectory + "/sound_" +
                                        std::to_string(id) + ext;
                result.generatedFiles.push_back(soundPath);
            }
        }
    }

    // 7. Create stage camera
    CreateStageCamera(doc, world);
    result.entitiesCreated++;  // Count the camera entity

    result.success = true;
    ENJIN_LOG_INFO(Assets, "SWF conversion complete: %u entities, %u shapes rasterized, "
                           "%u sprites converted, %u sounds extracted, %u warnings",
                   result.entitiesCreated, result.shapesRasterized,
                   result.spritesConverted, result.soundsExtracted,
                   static_cast<u32>(result.warnings.size()));

    return result;
}

} // namespace Assets
} // namespace Enjin
