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
    // A scene that never asked for GI and has no bake is not a gap, it is a
    // choice -- plenty of scenes are lit by their direct lights and want
    // nothing else. Only the scene that ASKED and did not get it has something
    // to report, and that is the branch above.
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

ReflectionSource ResolveReflections(bool sceneWantsTraced, const ReflectionCapabilities& caps) {
    if (sceneWantsTraced && caps.canTrace) return ReflectionSource::Traced;
    // The environment term needs a dome to sample. A scene with a configured
    // sky already has one; a scene that wants reflections gets one derived from
    // its ambient light instead.
    if (caps.hasConfiguredSky || sceneWantsTraced) return ReflectionSource::Environment;
    return ReflectionSource::None;
}

EnvironmentDome DeriveEnvironmentDome(const Math::Vector3& ambientColor) {
    EnvironmentDome dome;
    // A dome that is brighter above than below, which is what every real
    // environment does and what makes a reflection read as a direction rather
    // than a tint. The factors are a shape, not a colour: everything here is
    // the scene's own ambient scaled, so a black ambient stays black.
    dome.top = Math::Vector3(ambientColor.x * 1.35f, ambientColor.y * 1.35f, ambientColor.z * 1.45f);
    dome.horizon = ambientColor;
    dome.bottom = Math::Vector3(ambientColor.x * 0.55f, ambientColor.y * 0.55f, ambientColor.z * 0.5f);
    return dome;
}

ShadowSource ResolveShadows(bool sceneWantsTraced, const ShadowCapabilities& caps) {
    if (sceneWantsTraced && caps.canTrace) return ShadowSource::Traced;
    if (caps.shadowMapsAvailable) return ShadowSource::ShadowMaps;
    // A lightmap froze the static shadows in at bake time. Better than nothing,
    // and worth naming separately: anything that MOVES casts nothing at all,
    // which is a different picture rather than a dimmer one.
    if (caps.hasBakedLightmap) return ShadowSource::BakedOnly;
    return ShadowSource::None;
}

bool ShouldRestoreShadowMaps(bool sceneWantsTraced, bool shadowMapsEnabled, bool canTrace) {
    // Both off is a scene that wanted no shadows. Only the combination of
    // 'traced was asked for' and 'this backend cannot' says the maps were
    // switched off to avoid paying twice rather than as a look.
    return sceneWantsTraced && !canTrace && !shadowMapsEnabled;
}

} // namespace Renderer
} // namespace Enjin
