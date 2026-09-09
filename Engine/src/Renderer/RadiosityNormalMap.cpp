#include "Enjin/Renderer/RadiosityNormalMap.h"

#include <cmath>

namespace Enjin {
namespace Renderer {

namespace {
// 1/sqrt(6), 1/sqrt(2), 1/sqrt(3) and 2/sqrt(6), written out rather than
// computed so the shader can carry the same literals and the two cannot drift.
constexpr f32 kA = 0.408248290f;   // 1/sqrt(6)
constexpr f32 kB = 0.707106781f;   // 1/sqrt(2)
constexpr f32 kC = 0.577350269f;   // 1/sqrt(3)
constexpr f32 kD = 0.816496581f;   // 2/sqrt(6) = sqrt(2/3)

const Math::Vector3 kBasis[kRNMBasisCount] = {
    Math::Vector3(-kA, -kB, kC),
    Math::Vector3(-kA,  kB, kC),
    Math::Vector3( kD, 0.0f, kC),
};
} // namespace

const Math::Vector3* RNMBasis() { return kBasis; }

Math::Vector3 RNMWeights(const Math::Vector3& tangentSpaceNormal) {
    Math::Vector3 n = tangentSpaceNormal;
    const f32 len = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
    // A zero or broken normal resolves to the flat answer rather than to
    // nothing. An unnormalized normal map, or a vertex with no tangent frame,
    // would otherwise produce a black texel that looks like missing light.
    if (!(len > 1e-6f)) return Math::Vector3(1.0f / 3.0f, 1.0f / 3.0f, 1.0f / 3.0f);
    n = Math::Vector3(n.x / len, n.y / len, n.z / len);

    f32 w[kRNMBasisCount];
    f32 sum = 0.0f;
    for (u32 i = 0; i < kRNMBasisCount; ++i) {
        const f32 d = n.x * kBasis[i].x + n.y * kBasis[i].y + n.z * kBasis[i].z;
        w[i] = d > 0.0f ? d : 0.0f;
        sum += w[i];
    }

    // Only reachable for a normal pointing INTO the surface, which is authored
    // data being wrong rather than a case to handle gracefully in the shader.
    // Flat is the honest answer: it is what the texel was baked as.
    if (!(sum > 1e-6f)) return Math::Vector3(1.0f / 3.0f, 1.0f / 3.0f, 1.0f / 3.0f);

    return Math::Vector3(w[0] / sum, w[1] / sum, w[2] / sum);
}

Math::Vector3 RNMResolve(const Math::Vector3& weights,
                         const Math::Vector3& basis0,
                         const Math::Vector3& basis1,
                         const Math::Vector3& basis2) {
    return Math::Vector3(
        basis0.x * weights.x + basis1.x * weights.y + basis2.x * weights.z,
        basis0.y * weights.x + basis1.y * weights.y + basis2.y * weights.z,
        basis0.z * weights.x + basis1.z * weights.y + basis2.z * weights.z);
}

} // namespace Renderer
} // namespace Enjin
