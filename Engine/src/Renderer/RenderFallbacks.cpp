#include "Enjin/Renderer/RenderFallbacks.h"

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

FogSubstitution SubstituteVolumetricFog(const VolumetricFogRequest& request,
                                        bool canRunVolumetric,
                                        f32 existingFogDensity) {
    FogSubstitution out;
    if (!request.enabled || canRunVolumetric) return out;
    if (!(request.density > 0.0f)) return out;   // nothing to stand in for

    // The scene already has analytic fog its author tuned. Adding more on top
    // would double it, and the tuned value is the one that was judged by eye.
    if (existingFogDensity > 1e-4f) return out;

    out.apply = true;
    out.color = request.color;
    out.heightFalloff = request.heightFalloff;

    // A volume fogs everything from the camera outward, so the stand-in starts
    // at zero rather than at whatever distance the scene's unused analytic fog
    // happened to default to.
    out.start = 0.0f;
    // Full fog at the distance where a volume of this density would have
    // absorbed about ninety percent of the light: exp(-density * d) = 0.1, so
    // d = ln(10) / density. It is an approximation of an exponential by a ramp,
    // chosen because it can be explained rather than tuned.
    out.end = 2.302585f / request.density;
    // Density here is the shader's maximum STRENGTH, not extinction per unit --
    // the falloff is already carried by the range above. Full strength at the
    // far end is what makes the two agree at the one distance they can.
    out.density = 1.0f;
    return out;
}

} // namespace Renderer
} // namespace Enjin
