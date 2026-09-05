#pragma once

#include "Enjin/Platform/Platform.h"

#include <string>
#include <vector>

namespace Enjin {

namespace ECS { class World; }
namespace Renderer { struct SceneRenderSettings; }

namespace Scene {

// Walks the live World for components that reference asset files on disk and
// returns one human-readable warning per missing file, e.g.
//   "Missing texture: textures/rock.png (entity 42, Material)"
//
// Relative paths are tried against searchRoots in order (callers pass the
// project root first, then the scene file's directory — the same order
// BuildPipeline::ValidateAssets uses). Absolute paths are checked directly.
// Empty path fields are skipped; empty searchRoots returns no warnings
// (nothing to resolve against). Paths that escape every root also report as
// missing, which is the right warning for a save-time check.
//
// NOTE: keep the component field list in sync with the JSON scan in
// BuildPipeline::ValidateAssets (Engine/src/Build/BuildPipeline.cpp).
ENJIN_API std::vector<std::string> FindMissingAssetPaths(
    ECS::World* world,
    const std::vector<std::string>& searchRoots);

// Scene-wide render settings that combine into something inert or invisible.
// Same rule as the component checks above: each field is individually valid and
// it is the PAIRING that is wrong, so only the combination can be judged.
ENJIN_API std::vector<std::string> FindRenderSettingsWarnings(
    const Renderer::SceneRenderSettings& settings);

} // namespace Scene
} // namespace Enjin
