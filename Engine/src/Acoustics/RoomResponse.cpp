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

    // Reject a crossing that lands in the truncation artifact.
    //
    // This is the one that mattered. A backward integral over a response cut
    // off at T always falls to zero AS IT APPROACHES T, because there is no
    // energy left in the sum -- so -35 dB is always crossed eventually, near
    // the end, no matter how long the room actually rings. The curve there
    // describes where the recording stopped, not where the sound did.
    //
    // The last fifth of the window is therefore unusable, and a fit that
    // depends on it is not a measurement. Refusing here is what turns "this
    // room is 9 seconds" (wrong, and unfalsifiable) into "this room is longer
    // than I can measure" (true, and actionable). Rooms whose decay resolves
    // well inside the window are unaffected.
    const f32 windowEnd = static_cast<f32>(schroeder.size()) * binSeconds;
    if (t35 > windowEnd * 0.8f) return 0.0f;

    return 2.0f * (t35 - t5);
}

} // namespace

f32 SabineRT60(f32 volume, f32 surfaceArea, f32 averageAbsorption) {
    const f32 absorbingArea = surfaceArea * averageAbsorption;
    if (absorbingArea <= 1.0e-6f || volume <= 0.0f) return 0.0f;
    return 0.161f * volume / absorbingArea;
}

namespace {

// A cheap probe of the room, from a handful of rays.
//
// This exists to size the measurement window, and it has to be measured rather
// than computed from the geometry. The first version of it derived volume from
// the triangles via the divergence theorem, which is exact for a room modelled
// as a single closed shell -- and wrong for every real scene, because a real
// scene is built from SOLID SLABS. Summing the signed volume of a building made
// of wall boxes gives the volume of the walls, not of the rooms inside them, so
// the estimate came out tiny and the window came out short, which is precisely
// the failure the window was added to fix. It would have passed the synthetic
// tests and failed in the editor.
//
// Rays do not care how the scene is modelled. A short pass gives the mean free
// path directly, and the average absorption a ray actually meets, and those two
// are all a decay estimate needs:
//
//   bounces to fall 60 dB = 60 / (-10 log10(1 - a))
//   RT60 = bounces * meanFreePath / c
//
// No volume, no surface area, no assumption about how anything was authored.
struct RoomProbe {
    f32 meanFreePath = 8.0f;
    f32 meanAbsorption = 0.1f;
    f32 rt60 = 0.0f;
    bool usable = false;
};

RoomProbe ProbeRoom(const AcousticBVH& bvh, const Audio::AcousticScene& scene,
                    const Math::Vector3& origin, u32 seed) {
    RoomProbe probe;
    constexpr u32 kProbeRays = 96;
    constexpr u32 kProbeBounces = 160;

    std::mt19937 rng(seed ^ 0x9E3779B9u);
    f64 pathTotal = 0.0;
    f64 absorptionTotal = 0.0;
    u32 segments = 0;

    for (u32 r = 0; r < kProbeRays; ++r) {
        Math::Vector3 position = origin;
        Math::Vector3 direction = UniformSphere(rng);

        for (u32 bounce = 0; bounce < kProbeBounces; ++bounce) {
            const RayHit hit = bvh.Raycast(position, direction, 1.0e6f);
            if (!hit.hit) break;            // escaped: contributes nothing

            pathTotal += hit.distance;
            ++segments;

            const usize count = scene.materials.Count();
            const usize mi = count ? std::min<usize>(
                static_cast<usize>(std::max(hit.material, 0)), count - 1) : 0;
            if (count) {
                const auto& props = scene.materials.At(mi);
                // The mid band sets the window: it is the band the fit reports
                // and it sits between the other two, so a window that resolves
                // it resolves the low band to within a factor the clamp covers.
                absorptionTotal += static_cast<f64>(props.absorption[1]);
            }

            // Purely specular, deliberately. The probe is measuring how far a
            // ray goes between surfaces, and scattering costs a random draw per
            // bounce to produce the same average free path.
            position = Math::Vector3(hit.point.x + hit.normal.x * 1.0e-3f,
                                     hit.point.y + hit.normal.y * 1.0e-3f,
                                     hit.point.z + hit.normal.z * 1.0e-3f);
            const f32 d = Dot(direction, hit.normal);
            direction = Normalize(Math::Vector3(direction.x - 2.0f * d * hit.normal.x,
                                                direction.y - 2.0f * d * hit.normal.y,
                                                direction.z - 2.0f * d * hit.normal.z));
        }
    }

    if (segments < 8) return probe;          // nothing enclosed enough to measure

    probe.meanFreePath = static_cast<f32>(pathTotal / segments);
    probe.meanAbsorption = static_cast<f32>(absorptionTotal / segments);
    probe.usable = probe.meanFreePath > 0.01f;

    // Bounces to fall 60 dB at this average absorption, times the time per
    // bounce. Clamped away from a = 0 (a perfectly reflective room never
    // decays, and no window is long enough for that) and from a = 1.
    const f32 alpha = std::min(std::max(probe.meanAbsorption, 0.002f), 0.99f);
    const f32 dbPerBounce = -10.0f * std::log10(1.0f - alpha);
    if (dbPerBounce > 1.0e-5f) {
        const f32 bounces = 60.0f / dbPerBounce;
        probe.rt60 = bounces * probe.meanFreePath / kSpeedOfSound;
    }
    return probe;
}

} // namespace

RoomResponse TraceRoomResponse(const AcousticBVH& bvh, const Audio::AcousticScene& scene,
                               const Math::Vector3& origin, const RoomTraceSettings& settings) {
    RoomResponse response;
    if (!bvh.IsBuilt() || settings.rayCount == 0) return response;

    // Size the window to the room, unless the caller insisted otherwise.
    //
    // 1.6x the estimate gives the T30 fit room to find -35 dB well clear of the
    // last fifth of the window, which is the part the truncation artifact eats.
    // The bounce budget has to grow with it or the rays run out before the
    // window does, which is the same failure wearing a different hat: concrete
    // absorbs 2% a bounce, so a 40 second tail is thousands of bounces.
    f32 windowSeconds = settings.maxTime;
    u32 bounceBudget = settings.maxBounces;
    if (settings.autoWindow) {
        const RoomProbe probe = ProbeRoom(bvh, scene, origin, settings.seed);
        if (probe.usable && probe.rt60 > 0.0f) {
            windowSeconds = std::max(settings.maxTime, probe.rt60 * 1.6f);
            windowSeconds = std::min(windowSeconds, settings.maxWindowSeconds);

            // The bounce budget has to keep up with the window, or the rays run
            // out before the clock does -- the same truncation wearing a
            // different hat. Concrete absorbs 2% a bounce, so a forty second
            // tail is several thousand bounces.
            const f32 needed = windowSeconds * kSpeedOfSound / probe.meanFreePath * 1.5f;
            bounceBudget = std::max(settings.maxBounces,
                                    static_cast<u32>(std::min(needed, 200000.0f)));
        }
    }

    const usize bins = static_cast<usize>(
        std::max(1.0f, std::ceil(windowSeconds / std::max(settings.binSeconds, 1.0e-4f))));
    std::vector<std::vector<f32>> histogram(Audio::kAcousticBands, std::vector<f32>(bins, 0.0f));

    std::mt19937 rng(settings.seed);
    std::uniform_real_distribution<f32> unit(0.0f, 1.0f);

    f64 pathTotal = 0.0;
    u32 pathSegments = 0;
    f64 depositedEnergy = 0.0;
    f32 firstReflection = windowSeconds;

    for (u32 r = 0; r < settings.rayCount; ++r) {
        Math::Vector3 position = origin;
        Math::Vector3 direction = UniformSphere(rng);
        f32 energy[Audio::kAcousticBands] = {1.0f, 1.0f, 1.0f};
        f32 travelled = 0.0f;

        bool finished = false;
        for (u32 bounce = 0; bounce < bounceBudget; ++bounce) {
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
            if (arrival >= windowSeconds) { finished = true; break; }

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
    response.firstReflection = (firstReflection < windowSeconds) ? firstReflection : 0.0f;
    return response;
}

} // namespace Acoustics
} // namespace Enjin
