#include "Enjin/Acoustics/EarlyReflections.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <unordered_set>

namespace Enjin {
namespace Acoustics {

namespace {

f32 Dot(const Math::Vector3& a, const Math::Vector3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Math::Vector3 Sub(const Math::Vector3& a, const Math::Vector3& b) {
    return Math::Vector3(a.x - b.x, a.y - b.y, a.z - b.z);
}

f32 Length(const Math::Vector3& v) { return std::sqrt(Dot(v, v)); }

Math::Vector3 Normalize(const Math::Vector3& v) {
    const f32 len = Length(v);
    if (len < 1.0e-12f) return Math::Vector3(0.0f, 1.0f, 0.0f);
    return Math::Vector3(v.x / len, v.y / len, v.z / len);
}

Math::Vector3 UniformSphere(std::mt19937& rng) {
    std::uniform_real_distribution<f32> u(-1.0f, 1.0f);
    std::uniform_real_distribution<f32> a(0.0f, 6.28318531f);
    const f32 z = u(rng);
    const f32 r = std::sqrt(std::max(0.0f, 1.0f - z * z));
    const f32 phi = a(rng);
    return Math::Vector3(r * std::cos(phi), r * std::sin(phi), z);
}

// One triangle of the scene, with the plane it lies in.
struct Surface {
    Math::Vector3 a, b, c;
    Math::Vector3 normal;      // unit
    f32 d = 0.0f;              // plane: dot(normal, p) + d = 0
    i32 material = 0;
};

Surface MakeSurface(const Audio::AcousticScene& scene, u32 tri) {
    Surface s;
    s.a = scene.vertices[static_cast<usize>(scene.indices[tri * 3 + 0])];
    s.b = scene.vertices[static_cast<usize>(scene.indices[tri * 3 + 1])];
    s.c = scene.vertices[static_cast<usize>(scene.indices[tri * 3 + 2])];
    const Math::Vector3 e1 = Sub(s.b, s.a);
    const Math::Vector3 e2 = Sub(s.c, s.a);
    s.normal = Normalize(Math::Vector3(e1.y * e2.z - e1.z * e2.y,
                                       e1.z * e2.x - e1.x * e2.z,
                                       e1.x * e2.y - e1.y * e2.x));
    s.d = -Dot(s.normal, s.a);
    s.material = (tri < scene.materialIndices.size()) ? scene.materialIndices[tri] : 0;
    return s;
}

f32 PlaneDistance(const Surface& s, const Math::Vector3& p) {
    return Dot(s.normal, p) + s.d;
}

Math::Vector3 MirrorAcross(const Surface& s, const Math::Vector3& p) {
    const f32 dist = PlaneDistance(s, p);
    return Math::Vector3(p.x - 2.0f * dist * s.normal.x,
                         p.y - 2.0f * dist * s.normal.y,
                         p.z - 2.0f * dist * s.normal.z);
}

// Where a segment crosses a plane, and whether it lands inside the triangle.
//
// Barycentric rather than a ray cast, because the point is known to be on the
// plane already and what is being asked is only whether it is inside the three
// edges. A cast would also find OTHER triangles and answer a different
// question.
bool SegmentHitsTriangle(const Surface& s, const Math::Vector3& from, const Math::Vector3& to,
                         Math::Vector3& out) {
    const f32 dFrom = PlaneDistance(s, from);
    const f32 dTo = PlaneDistance(s, to);
    // Both ends on the same side: the segment never crosses.
    if ((dFrom > 0.0f) == (dTo > 0.0f)) return false;
    const f32 denom = dFrom - dTo;
    if (std::fabs(denom) < 1.0e-12f) return false;

    const f32 t = dFrom / denom;
    const Math::Vector3 p(from.x + (to.x - from.x) * t,
                          from.y + (to.y - from.y) * t,
                          from.z + (to.z - from.z) * t);

    const Math::Vector3 v0 = Sub(s.c, s.a);
    const Math::Vector3 v1 = Sub(s.b, s.a);
    const Math::Vector3 v2 = Sub(p, s.a);
    const f32 d00 = Dot(v0, v0), d01 = Dot(v0, v1), d02 = Dot(v0, v2);
    const f32 d11 = Dot(v1, v1), d12 = Dot(v1, v2);
    const f32 inv = d00 * d11 - d01 * d01;
    if (std::fabs(inv) < 1.0e-12f) return false;   // degenerate triangle
    const f32 u = (d11 * d02 - d01 * d12) / inv;
    const f32 v = (d00 * d12 - d01 * d02) / inv;
    if (u < 0.0f || v < 0.0f || u + v > 1.0f) return false;

    out = p;
    return true;
}

// Amplitude left after bouncing off this surface, per band.
//
// Absorption is an ENERGY coefficient, so the amplitude factor is the square
// root of what is left. Using (1 - alpha) directly on amplitude would make
// every surface roughly twice as absorbent as it is, and a concrete room would
// sound like a carpeted one.
void ApplyAbsorption(const Audio::AcousticProperties& props, f32* gain) {
    for (u32 band = 0; band < Audio::kAcousticBands; ++band) {
        gain[band] *= std::sqrt(std::max(0.0f, 1.0f - props.absorption[band]));
    }
}

const Audio::AcousticProperties& MaterialOf(const Audio::AcousticScene& scene, i32 index) {
    const usize count = scene.materials.Count();
    const usize i = (count == 0) ? 0
                                 : std::min(static_cast<usize>(std::max(index, 0)), count - 1);
    static const Audio::AcousticProperties kFallback{};
    return (count == 0) ? kFallback : scene.materials.At(i);
}

} // namespace

EarlyReflectionResult TraceEarlyReflections(const AcousticBVH& bvh,
                                            const Audio::AcousticScene& scene,
                                            const Math::Vector3& source,
                                            const Math::Vector3& listener,
                                            const EarlyReflectionSettings& settings) {
    EarlyReflectionResult result;

    // --- the straight line ---------------------------------------------------
    result.directDistance = Length(Sub(listener, source));
    result.directDelay = result.directDistance / kSpeedOfSound;
    result.directGain = (result.directDistance > 1.0e-3f) ? (1.0f / result.directDistance) : 1.0f;
    result.directOccluded = bvh.IsBuilt() && bvh.Occluded(source, listener);

    if (!bvh.IsBuilt()) return result;

    // --- which surfaces are worth mirroring across ---------------------------
    //
    // Rays from the source find the surfaces that actually surround it. This is
    // the only sampled part: a surface missed by every ray contributes no tap,
    // but a surface that IS found is then handled exactly.
    std::vector<u32> candidates;
    {
        std::unordered_set<u32> seen;
        std::mt19937 rng(settings.seed);
        for (u32 i = 0; i < settings.candidateRays && candidates.size() < settings.maxCandidates;
             ++i) {
            const RayHit hit = bvh.Raycast(source, UniformSphere(rng), 1.0e6f);
            if (!hit.hit) continue;
            if (seen.insert(hit.triangle).second) candidates.push_back(hit.triangle);
        }
    }
    if (candidates.empty()) return result;

    std::vector<Surface> surfaces;
    surfaces.reserve(candidates.size());
    for (u32 tri : candidates) surfaces.push_back(MakeSurface(scene, tri));

    // --- first order ---------------------------------------------------------
    struct Image {
        Math::Vector3 position;     // the mirrored source
        usize surface;              // which surface produced it
        f32 gain[Audio::kAcousticBands];
    };
    std::vector<Image> firstOrder;
    firstOrder.reserve(surfaces.size());

    for (usize si = 0; si < surfaces.size(); ++si) {
        const Surface& s = surfaces[si];

        // The source has to be on the reflecting side. A surface the source is
        // behind cannot reflect it, and mirroring anyway produces a plausible
        // tap from a wall on the far side of a floor.
        const f32 sourceSide = PlaneDistance(s, source);
        const f32 listenerSide = PlaneDistance(s, listener);
        if (std::fabs(sourceSide) < 1.0e-4f) continue;
        if ((sourceSide > 0.0f) != (listenerSide > 0.0f)) continue;

        const Math::Vector3 image = MirrorAcross(s, source);

        Math::Vector3 point;
        if (!SegmentHitsTriangle(s, listener, image, point)) continue;

        // Both legs have to be clear. The reflection is only heard if the sound
        // reached the wall AND got back.
        if (bvh.Occluded(source, point)) continue;
        if (bvh.Occluded(point, listener)) continue;

        const f32 distance = Length(Sub(point, source)) + Length(Sub(listener, point));
        if (distance < 1.0e-3f) continue;

        EarlyReflection tap;
        tap.order = 1;
        tap.distance = distance;
        tap.delay = distance / kSpeedOfSound;
        tap.direction = Normalize(Sub(point, listener));
        tap.reflectionPoint = point;
        const f32 spread = 1.0f / distance;
        for (u32 band = 0; band < Audio::kAcousticBands; ++band) tap.gain[band] = spread;
        ApplyAbsorption(MaterialOf(scene, s.material), tap.gain);

        Image img;
        img.position = image;
        img.surface = si;
        for (u32 band = 0; band < Audio::kAcousticBands; ++band) img.gain[band] = tap.gain[band];
        firstOrder.push_back(img);

        f32 loudest = 0.0f;
        for (u32 band = 0; band < Audio::kAcousticBands; ++band)
            loudest = std::max(loudest, tap.gain[band]);
        if (loudest >= settings.gainFloor) result.taps.push_back(tap);
    }

    // --- second order --------------------------------------------------------
    //
    // A first-order image mirrored again. This is what gives a corner its
    // character: two walls meeting send back a reflection neither would alone.
    // Both bounce points have to be validated and all three legs unoccluded,
    // which is why the candidate set is capped -- this is a pass over every
    // pair.
    if (settings.maxOrder >= 2) {
        for (const Image& first : firstOrder) {
            for (usize sj = 0; sj < surfaces.size(); ++sj) {
                if (sj == first.surface) continue;
                const Surface& s2 = surfaces[sj];

                const f32 imageSide = PlaneDistance(s2, first.position);
                const f32 listenerSide = PlaneDistance(s2, listener);
                if (std::fabs(imageSide) < 1.0e-4f) continue;
                if ((imageSide > 0.0f) != (listenerSide > 0.0f)) continue;

                const Math::Vector3 image2 = MirrorAcross(s2, first.position);

                // Second bounce: where the path from the listener meets s2.
                Math::Vector3 point2;
                if (!SegmentHitsTriangle(s2, listener, image2, point2)) continue;

                // First bounce: unfolding one step further, the path from that
                // point back towards the original image meets s1.
                const Surface& s1 = surfaces[first.surface];
                Math::Vector3 point1;
                if (!SegmentHitsTriangle(s1, point2, first.position, point1)) continue;

                if (bvh.Occluded(source, point1)) continue;
                if (bvh.Occluded(point1, point2)) continue;
                if (bvh.Occluded(point2, listener)) continue;

                const f32 distance = Length(Sub(point1, source)) +
                                     Length(Sub(point2, point1)) +
                                     Length(Sub(listener, point2));
                if (distance < 1.0e-3f) continue;

                EarlyReflection tap;
                tap.order = 2;
                tap.distance = distance;
                tap.delay = distance / kSpeedOfSound;
                tap.direction = Normalize(Sub(point2, listener));
                tap.reflectionPoint = point2;
                const f32 spread = 1.0f / distance;
                for (u32 band = 0; band < Audio::kAcousticBands; ++band) {
                    // The first surface's absorption is already in first.gain,
                    // but that gain also carries the FIRST path's spreading,
                    // which is wrong for this longer path. So the absorption is
                    // re-applied to a clean 1/distance instead of reused.
                    tap.gain[band] = spread;
                }
                ApplyAbsorption(MaterialOf(scene, s1.material), tap.gain);
                ApplyAbsorption(MaterialOf(scene, s2.material), tap.gain);

                f32 loudest = 0.0f;
                for (u32 band = 0; band < Audio::kAcousticBands; ++band)
                    loudest = std::max(loudest, tap.gain[band]);
                if (loudest >= settings.gainFloor) result.taps.push_back(tap);
            }
        }
    }

    // --- keep the ones that will be heard ------------------------------------
    //
    // By loudness, not by arrival: a quiet early tap matters less than a strong
    // later one, and a cap applied in time order would throw away the loudest
    // reflection in the room because it happened to bounce off something far
    // away.
    std::sort(result.taps.begin(), result.taps.end(),
              [](const EarlyReflection& a, const EarlyReflection& b) {
                  return a.gain[1] > b.gain[1];
              });
    if (result.taps.size() > settings.maxTaps) result.taps.resize(settings.maxTaps);

    // Then back into arrival order, which is the order anything rendering them
    // into a delay line wants.
    std::sort(result.taps.begin(), result.taps.end(),
              [](const EarlyReflection& a, const EarlyReflection& b) {
                  return a.delay < b.delay;
              });
    return result;
}

} // namespace Acoustics
} // namespace Enjin
