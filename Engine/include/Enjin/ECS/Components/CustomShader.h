#pragma once

#include "Enjin/Platform/Platform.h"
#include <string>

namespace Enjin {
namespace ECS {

// A shader-graph-generated custom shader assigned to this entity, persisted
// with the scene so "Apply to Selected Entity" survives save/load (and ships in
// exported games - the engine compiles GLSL at runtime). The render system's
// FlushPendingChanges re-applies any component that isn't live yet; pipelines
// dedup by source hash, so entities sharing one graph share one pipeline.
struct CustomShaderComponent {
    std::string vertexSource;     // generated GLSL (serialized)
    std::string fragmentSource;   // generated GLSL (serialized)
    std::string graphLabel;       // which graph produced it (informational)

    // --- Runtime (not serialized) ---
    bool applied = false;   // pipeline is live this session
    bool failed = false;    // compile failed once; don't retry every frame
};

} // namespace ECS
} // namespace Enjin
