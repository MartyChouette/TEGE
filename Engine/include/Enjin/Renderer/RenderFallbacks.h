#pragma once

// What a runtime uses when it cannot run what the scene asked for.
//
// Web parity is not porting the desktop path; it is having the right
// web-centric SUBSTITUTE for each capability. A browser cannot trace rays and
// cannot afford a froxel volume, so the answer is not "do it slower" -- it is a
// different technique that preserves what the author meant at a fraction of the
// cost. Baked light instead of traced light. Analytic fog instead of
// volumetric.
//
// Every substitution lives here so the family is visible in one place, and each
// one is decided by a pure function so it can be tested and so the editor can
// state what each platform will get without running either.
//
// --- Global illumination -----------------------------------------------------
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
#include "Enjin/Math/Vector.h"

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

// --- Fog ---------------------------------------------------------------------

// Volumetric fog is a froxel volume traced every frame: light shafts, drifting
// noise, a phase function. None of that exists on a backend that cannot afford
// it, and a scene authored around thick atmosphere arriving perfectly clear is
// a bigger change to how it reads than almost anything else on this list.
//
// The substitute is the analytic distance-and-height fog every backend already
// runs. It keeps the colour, the depth cue and the height gradient. It does NOT
// keep the shafts or the noise: this is an approximation that preserves the
// intent, not a cheaper way to compute the same thing.
struct VolumetricFogRequest {
    bool enabled = false;
    Math::Vector3 color = Math::Vector3(0.9f, 0.9f, 1.0f);
    f32 density = 0.0f;
    f32 heightFalloff = 0.0f;
};

struct FogSubstitution {
    bool apply = false;          // the runtime should adopt the values below
    Math::Vector3 color;
    // The analytic fog is LINEAR between start and end, scaled by density as a
    // maximum strength -- so a range comes with the substitution or it does
    // nothing at all. The first version passed density through and left the
    // authored 20-unit start alone, which meant a room 13 units from the camera
    // received no fog whatever while every log line claimed success.
    f32 density = 0.0f;
    f32 start = 0.0f;
    f32 end = 0.0f;
    f32 heightFalloff = 0.0f;
};

// `existingFogDensity` is the analytic fog the scene ALREADY authored. When a
// scene has both, the analytic one is what its author tuned by eye on this very
// backend, and substituting on top of it would double the fog -- so the
// substitution stands down rather than compete.
FogSubstitution SubstituteVolumetricFog(const VolumetricFogRequest& request,
                                        bool canRunVolumetric,
                                        f32 existingFogDensity);

// --- Reflections -------------------------------------------------------------

// Ray-traced reflections are the desktop answer. A browser cannot trace, and
// the cheap screen-space substitute is deliberately NOT taken: a screen-space
// reflection can only show what is already on screen, so it admits that the
// world stops at the frame edge. That is the one thing this whole family of
// techniques exists to avoid.
//
// What web gets instead is an environment reflection sampled from the sky dome
// along the reflection vector. It is world-space, it does not care where the
// frame ends, and it costs no rays.
enum class ReflectionSource : u8 {
    None = 0,
    Traced,        // ray-traced, desktop
    Environment,   // sky-dome IBL along the reflection vector
};

struct ReflectionCapabilities {
    bool canTrace = false;
    // The scene configured a sky, which is what the environment term samples.
    bool hasConfiguredSky = false;
};

ReflectionSource ResolveReflections(bool sceneWantsTraced, const ReflectionCapabilities& caps);

// A sky dome derived from the scene's own ambient light, for a scene that wants
// reflections and never configured a sky.
//
// Derived, not invented. A plausible blue sky would be a colour nobody in the
// scene chose, and would light a night interior like an afternoon. Ambient is
// the one environment value every scene really has, so a black ambient yields a
// black dome and the substitution correctly amounts to nothing.
struct EnvironmentDome {
    Math::Vector3 top;
    Math::Vector3 horizon;
    Math::Vector3 bottom;
};
EnvironmentDome DeriveEnvironmentDome(const Math::Vector3& ambientColor);

} // namespace Renderer
} // namespace Enjin
