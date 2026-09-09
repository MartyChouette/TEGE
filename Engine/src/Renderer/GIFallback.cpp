#include "Enjin/Renderer/GIFallback.h"

namespace Enjin {
namespace Renderer {

GISource ResolveGI(bool sceneWantsDynamic, const GICapabilities& caps) {
    if (sceneWantsDynamic && caps.canRunDynamic) return GISource::Dynamic;
    // The substitution. A backend that cannot trace still has an answer, as
    // long as somebody baked one.
    if (caps.hasBakedLightmap) return GISource::Baked;
    return GISource::None;
}

const char* GISourceName(GISource source) {
    switch (source) {
        case GISource::Dynamic: return "dynamic (DDGI)";
        case GISource::Baked:   return "baked lightmap";
        default:                return "none";
    }
}

const char* DescribeGIGap(bool sceneWantsDynamic, const GICapabilities& caps) {
    if (ResolveGI(sceneWantsDynamic, caps) != GISource::None) return "";
    if (sceneWantsDynamic && !caps.canRunDynamic) {
        return "This scene asks for dynamic GI, which this backend cannot run, "
               "and it has no baked lightmap to fall back to. It will render "
               "with no global illumination. Bake one: Tools > Art & Animation "
               "> Bake Lightmap.";
    }
    if (!sceneWantsDynamic) {
        return "This scene has no global illumination: dynamic GI is off and "
               "nothing is baked.";
    }
    return "";
}

} // namespace Renderer
} // namespace Enjin
