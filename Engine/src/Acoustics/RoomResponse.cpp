#include "Enjin/Acoustics/RoomResponse.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace Enjin {
namespace Acoustics {

namespace {

Math::Vector3 Normalize(const Math::Vector3& v) {
    const f32 len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (len < 1.0e-12f) return Math::Vector3(0.0f, 1.0f, 0.0f);
    return Math::Vector3(v.x / len, v.y / len, v.z / len);
}

f32 Dot(const Math::Vector3& a, const Math::Vector3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

// A direction from the unit sphere, uniformly.
//
// The obvious way -- random angles -- clusters at the poles, which would send
// more rays at the floor and ceiling than at the walls and make every room's
// decay depend on which way is up.
Math::Vector3 UniformSphere(std::mt19937& rng) {
    std::uniform_real_distribution<f32> u(-1.0f, 1.0f);
    std::uniform_real_distribution<f32> a(0.0f, 6.28318531f);
    const f32 z = u(rng);
    const f32 r = std::sqrt(std::max(0.0f, 1.0f - z * z));
    const f32 phi = a(rng);
    return Math::Vector3(r * std::cos(phi), r * std::sin(phi), z);
}

// A direction in the hemisphere around a normal, cosine weighted -- which is
// what a diffusely scattering surface does.
Math::Vector3 CosineHemisphere(const Math::Vector3& n, std::mt19937& rng) {
    Math::Vector3 d = UniformSphere(rng);
    // Adding the normal to a uniform sphere point gives a cosine distribution
    // around it, which is both correct and cheaper than building a basis.
    d = Math::Vector3(d.x + n.x, d.y + n.y, d.z + n.z);
    if (d.x * d.x + d.y * d.y + d.z * d.z < 1.0e-8f) return n;
    return Normalize(d);
}

// RT60 from an energy histogram, by the method a measurement microphone uses.
//
// Schroeder backward integration turns a noisy decay into a smooth one: at each
// time, the energy STILL TO COME. Then a line is fitted between -5 dB and
// -35 dB and extrapolated to -60, which is the standard T30 measure. Fitting
// the whole curve to -60 directly would be at the mercy of the last few rays,
// where a handful of samples carry the entire tail.
f32 EstimateRT60(const std::vector<f32>& histogram, f32 binSeconds) {
    if (histogram.empty() || binSeconds <= 0.0f) return 0.0f;

    std::vector<f64> schroeder(histogram.size(), 0.0);
    f64 running = 0.0;
    for (usize i = histogram.size(); i-- > 0;) {
        running += static_cast<f64>(histogram[i]);
        schroeder[i] = running;
    }
    if (running <= 0.0) return 0.0;

    const f64 total = schroeder[0];
    if (total <= 0.0) return 0.0;

    // Times at which the remaining energy has fallen 5 dB and 35 dB.
    f32 t5 = -1.0f, t35 = -1.0f;
    for (usize i = 0; i < schroeder.size(); ++i) {
        const f64 ratio = schroeder[i] / total;
        if (ratio <= 0.0) break;
        const f64 db = 10.0 * std::log10(ratio);
        if (t5 < 0.0f && db <= -5.0) t5 = static_cast<f32>(i) * binSeconds;
        if (db <= -35.0) { t35 = static_cast<f32>(i) * binSeconds; break; }
    }

    // Never reached -35 dB inside the window: the tail is longer than we
    // measured. Reporting the window length would claim a short room; reporting
    // nothing is honest, and the caller treats it as "could not measure".
    if (t5 < 0.0f || t35 < 0.0f || t35 <= t5) return 0.0f;

    return 2.0f * (t35 - t5);
}

} // namespace

f32 SabineRT60(f32 volume, f32 surfaceArea, f32 averageAbsorption) {
    const f32 absorbingArea = surfaceArea * averageAbsorption;
    if (absorbingArea <= 1.0e-6f || volume <= 0.0f) return 0.0f;
    return 0.161f * volume / absorbingArea;
}

RoomResponse TraceRoomResponse(const AcousticBVH& bvh, const Audio::AcousticScene& scene,
                               const Math::Vector3& origin, const RoomTraceSettings& settings) {
    RoomResponse response;
    if (!bvh.IsBuilt() || settings.rayCount == 0) return response;

    const usize bins = static_cast<usize>(
        std::max(1.0f, std::ceil(settings.maxTime / std::max(settings.binSeconds, 1.0e-4f))));
    std::vector<std::vector<f32>> histogram(Audio::kAcousticBands, std::vector<f32>(bins, 0.0f));

    std::mt19937 rng(settings.seed);
    std::uniform_real_distribution<f32> unit(0.0f, 1.0f);

    f64 pathTotal = 0.0;
    u32 pathSegments = 0;
    f64 depositedEnergy = 0.0;
    f32 firstReflection = settings.maxTime;

    for (u32 r = 0; r < settings.rayCount; ++r) {
        Math::Vector3 position = origin;
        Math::Vector3 direction = UniformSphere(rng);
        f32 energy[Audio::kAcousticBands] = {1.0f, 1.0f, 1.0f};
        f32 travelled = 0.0f;

        bool finished = false;
        for (u32 bounce = 0; bounce < settings.maxBounces; ++bounce) {
            const RayHit hit = bvh.Raycast(position, direction, 1.0e6f);
            if (!hit.hit) {
                // Out through a gap. An open scene is not an error -- it is
                // outdoors, and outdoors is exactly a room that returns almost
                // nothing.
                ++response.raysEscaped;
                finished = true;
                break;
            }

            travelled += hit.distance;
            const f32 arrival = travelled / kSpeedOfSound;
            // Past the window is a finished ray: it was followed for as long as
            // anyone is measuring, which is not the same as running out of
            // budget.
            if (arrival >= settings.maxTime) { finished = true; break; }

            pathTotal += hit.distance;
            ++pathSegments;
            if (bounce == 0) firstReflection = std::min(firstReflection, arrival);

            const Audio::AcousticProperties& props =
                scene.materials.At(static_cast<usize>(
                    std::min<usize>(static_cast<usize>(std::max(hit.material, 0)),
                                    scene.materials.Count() ? scene.materials.Count() - 1 : 0)));

            const usize bin = static_cast<usize>(arrival / settings.binSeconds);
            f32 remaining = 0.0f;
            for (u32 band = 0; band < Audio::kAcousticBands; ++band) {
                // Energy absorbed at this surface never comes back, so it is
                // removed BEFORE the deposit -- what lands in the histogram is
                // what the room still has.
                energy[band] *= (1.0f - props.absorption[band]);
                if (bin < bins) {
                    histogram[band][bin] += energy[band];
                    depositedEnergy += static_cast<f64>(energy[band]);
                }
                remaining = std::max(remaining, energy[band]);
            }
            if (remaining < settings.energyFloor) { finished = true; break; }

            // Specular or scattered, by the surface's scattering coefficient.
            // A polished wall mirrors; a rough one spreads. Choosing per bounce
            // rather than blending both is what keeps a ray a ray.
            position = Math::Vector3(hit.point.x + hit.normal.x * 1.0e-3f,
                                     hit.point.y + hit.normal.y * 1.0e-3f,
                                     hit.point.z + hit.normal.z * 1.0e-3f);
            if (unit(rng) < props.scattering) {
                direction = CosineHemisphere(hit.normal, rng);
            } else {
                const f32 d = Dot(direction, hit.normal);
                direction = Normalize(Math::Vector3(direction.x - 2.0f * d * hit.normal.x,
                                                    direction.y - 2.0f * d * hit.normal.y,
                                                    direction.z - 2.0f * d * hit.normal.z));
            }
        }

        // Fell out of the loop with energy left: the budget ended this ray, not
        // the room.
        if (!finished) ++response.raysTruncated;
    }

    response.raysTraced = settings.rayCount;
    for (u32 band = 0; band < Audio::kAcousticBands; ++band) {
        response.rt60[band] = EstimateRT60(histogram[band], settings.binSeconds);
    }
    response.meanFreePath = (pathSegments > 0)
                                ? static_cast<f32>(pathTotal / static_cast<f64>(pathSegments))
                                : 0.0f;
    // Per ray, so it is comparable between traces of different ray counts.
    response.reflectedEnergy = static_cast<f32>(depositedEnergy /
                                                static_cast<f64>(settings.rayCount));
    response.firstReflection = (firstReflection < settings.maxTime) ? firstReflection : 0.0f;
    return response;
}

} // namespace Acoustics
} // namespace Enjin
