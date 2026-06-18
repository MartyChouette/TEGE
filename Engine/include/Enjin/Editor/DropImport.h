#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/Entity.h"
#include <string>

// Drag-and-drop import helpers, factored out of EditorLayer::OnFileDrop so the
// entity-creation behavior can be unit-tested without a live editor/window.

namespace Enjin {
namespace ECS { class World; }
namespace Editor {

// Spawn a sound-emitter entity (TransformComponent + AudioSourceComponent whose
// clipPath points at the file), named after the file stem. Returns the new
// entity, or INVALID_ENTITY if world is null.
ENJIN_API ECS::Entity CreateAudioSourceEntity(ECS::World* world, const std::string& filePath);

// Spawn a camera-facing sprite quad (quad mesh + blended MaterialComponent whose
// baseColorTexturePath points at the file), named after the file stem. Returns
// the new entity, or INVALID_ENTITY if world is null.
ENJIN_API ECS::Entity CreateSpriteEntity(ECS::World* world, const std::string& filePath);

} // namespace Editor
} // namespace Enjin
