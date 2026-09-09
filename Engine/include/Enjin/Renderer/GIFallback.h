#pragma once

// Which global illumination a runtime actually uses, and what it costs a scene
// when the answer is "none".
//
// A scene asks for dynamic GI by ticking DDGI. Desktop can run it. The web build
// cannot -- there is no ray tracing in a browser, and the whole DDGI path is
// compiled out there. Until now that meant a scene authored with GI simply
// arrived on web without any, and nobody decided that: it was what was left over.
//
// The substitution this file makes explicit is baked light. Radiosity normal
// mapping is not web's inferior copy of DDGI, it is web's ANSWER to the same
// question, arrived at differently: pay for the light once, offline, and ship
// the answer. Every technique in this family works that way, which is why the
// right shape for web parity is a per-capability substitute rather than a port.
//
// Pure and free of both renderer and ECS, so the decision can be tested, and so
// the editor can state what each platform will get without running either one.

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"

namespace Enjin {
namespace Renderer {

enum class GISource : u8 {
    None = 0,     // the scene will render with no global illumination at all
    Dynamic,      // DDGI, traced at run time
    Baked,        // three-basis lightmaps
};

// What a particular runtime can offer.
struct GICapabilities {
    // This backend can run DDGI. False on WebGPU, and false on any desktop
    // build where the feature is unavailable.
    bool canRunDynamic = false;
    // The scene carries a baked lightmap: three atlases that loaded.
    bool hasBakedLightmap = false;
};

// The resolution itself.
//
// Baked wins over nothing, and dynamic wins over baked where it can run --
// dynamic responds to a scene that changes, which is the only reason to pay for
// it. A scene that asked for neither still uses a bake if one is present,
// because a lightmap in a project is there on purpose.
GISource ResolveGI(bool sceneWantsDynamic, const GICapabilities& caps);

// The name of a source, for logs and for the editor.
const char* GISourceName(GISource source);

// Empty when nothing is wrong. Otherwise, what this scene loses on this runtime
// and what to do about it.
//
// This exists because the failure it describes is invisible: a scene with no GI
// does not look broken, it looks flat, and flat is easy to blame on the art. A
// person deserves to be told before they ship it.
const char* DescribeGIGap(bool sceneWantsDynamic, const GICapabilities& caps);

} // namespace Renderer
} // namespace Enjin
