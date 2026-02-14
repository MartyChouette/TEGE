#include "Enjin/Effects/FourierMesh.h"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace Enjin {
namespace Effects {

void FourierMeshDecomposition::Decompose(const std::vector<Math::Vector2>& contour, i32 numCoefficients) {
    m_Coefficients.clear();
    m_OriginalContour = contour;

    i32 N = static_cast<i32>(contour.size());
    if (N < 3) return;

    // If numCoefficients is 0 or exceeds N, use all
    if (numCoefficients <= 0 || numCoefficients > N) {
        numCoefficients = N;
    }

    // Compute DFT coefficients from -numCoefficients/2 to +numCoefficients/2
    i32 halfK = numCoefficients / 2;

    for (i32 k = -halfK; k <= halfK; ++k) {
        f32 realPart = 0.0f;
        f32 imagPart = 0.0f;

        for (i32 n = 0; n < N; ++n) {
            // exp(-2*pi*i*k*n/N) = cos(angle) - i*sin(angle)
            f32 angle = -2.0f * Math::PI * static_cast<f32>(k) * static_cast<f32>(n) / static_cast<f32>(N);
            f32 cosA = Math::Cos(angle);
            f32 sinA = Math::Sin(angle);

            // Treat contour point as complex number: point.x + i*point.y
            // Multiply: (px + i*py) * (cos - i*sin) = (px*cos + py*sin) + i*(py*cos - px*sin)
            realPart += contour[n].x * cosA + contour[n].y * sinA;
            imagPart += contour[n].y * cosA - contour[n].x * sinA;
        }

        realPart /= static_cast<f32>(N);
        imagPart /= static_cast<f32>(N);

        FourierCoefficient coeff;
        coeff.real = realPart;
        coeff.imag = imagPart;
        coeff.frequency = k;
        coeff.amplitude = Math::Sqrt(realPart * realPart + imagPart * imagPart);
        coeff.phase = Math::Atan2(imagPart, realPart);

        m_Coefficients.push_back(coeff);
    }

    // Sort by amplitude (descending) so we can easily truncate
    std::sort(m_Coefficients.begin(), m_Coefficients.end(),
              [](const FourierCoefficient& a, const FourierCoefficient& b) {
                  return a.amplitude > b.amplitude;
              });
}

Math::Vector2 FourierMeshDecomposition::Reconstruct(f32 t, i32 numTerms) const {
    if (m_Coefficients.empty()) return Math::Vector2(0.0f, 0.0f);

    i32 maxTerms = static_cast<i32>(m_Coefficients.size());
    if (numTerms <= 0 || numTerms > maxTerms) {
        numTerms = maxTerms;
    }

    f32 x = 0.0f;
    f32 y = 0.0f;

    for (i32 i = 0; i < numTerms; ++i) {
        const auto& c = m_Coefficients[i];
        f32 angle = 2.0f * Math::PI * static_cast<f32>(c.frequency) * t;
        f32 cosA = Math::Cos(angle);
        f32 sinA = Math::Sin(angle);

        // Reconstruct: c_k * exp(2*pi*i*k*t) = (real + i*imag) * (cos + i*sin)
        // = (real*cos - imag*sin) + i*(real*sin + imag*cos)
        x += c.real * cosA - c.imag * sinA;
        y += c.real * sinA + c.imag * cosA;
    }

    return Math::Vector2(x, y);
}

std::vector<Math::Vector2> FourierMeshDecomposition::GenerateApproximation(i32 numTerms, i32 numSamples) const {
    std::vector<Math::Vector2> result;
    if (m_Coefficients.empty() || numSamples <= 0) return result;

    result.reserve(numSamples);
    for (i32 i = 0; i < numSamples; ++i) {
        f32 t = static_cast<f32>(i) / static_cast<f32>(numSamples);
        result.push_back(Reconstruct(t, numTerms));
    }

    return result;
}

f32 FourierMeshDecomposition::Animate(f32 time, i32 maxTerms) const {
    if (maxTerms <= 0) maxTerms = static_cast<i32>(m_Coefficients.size());

    // 1 term per second, clamped to maxTerms
    f32 terms = Math::Clamp(time, 0.0f, static_cast<f32>(maxTerms));
    return terms;
}

std::vector<Math::Vector2> FourierMeshDecomposition::GenerateAnimatedContour(f32 time, i32 maxTerms, i32 numSamples) const {
    f32 termCount = Animate(time, maxTerms);
    i32 intTerms = static_cast<i32>(termCount);
    if (intTerms < 1) intTerms = 1;

    // Generate approximation with the integer number of terms
    auto contour = GenerateApproximation(intTerms, numSamples);

    // If there's a fractional part, blend toward the next term
    f32 frac = termCount - static_cast<f32>(intTerms);
    if (frac > Math::EPSILON && intTerms < static_cast<i32>(m_Coefficients.size())) {
        auto contourNext = GenerateApproximation(intTerms + 1, numSamples);
        for (i32 i = 0; i < numSamples && i < static_cast<i32>(contour.size()); ++i) {
            contour[i] = Math::Lerp(contour[i], contourNext[i], frac);
        }
    }

    return contour;
}

// --- Triangulation Helpers ---

f32 FourierMeshDecomposition::Cross2D(const Math::Vector2& a, const Math::Vector2& b) {
    return a.x * b.y - a.y * b.x;
}

bool FourierMeshDecomposition::PointInTriangle(const Math::Vector2& p,
                                                const Math::Vector2& a,
                                                const Math::Vector2& b,
                                                const Math::Vector2& c) {
    f32 d1 = Cross2D(b - a, p - a);
    f32 d2 = Cross2D(c - b, p - b);
    f32 d3 = Cross2D(a - c, p - c);

    bool hasNeg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    bool hasPos = (d1 > 0) || (d2 > 0) || (d3 > 0);

    return !(hasNeg && hasPos);
}

bool FourierMeshDecomposition::IsEar(const std::vector<Math::Vector2>& polygon,
                                      i32 prev, i32 curr, i32 next) {
    const auto& a = polygon[prev];
    const auto& b = polygon[curr];
    const auto& c = polygon[next];

    // Check if the triangle is convex (counter-clockwise winding)
    if (Cross2D(b - a, c - a) <= 0.0f) {
        return false;
    }

    // Check that no other vertex lies inside this triangle
    i32 n = static_cast<i32>(polygon.size());
    for (i32 i = 0; i < n; ++i) {
        if (i == prev || i == curr || i == next) continue;
        if (PointInTriangle(polygon[i], a, b, c)) {
            return false;
        }
    }

    return true;
}

std::vector<u32> FourierMeshDecomposition::TriangulateContour(const std::vector<Math::Vector2>& contour) {
    std::vector<u32> indices;
    i32 n = static_cast<i32>(contour.size());
    if (n < 3) return indices;

    // Build index list
    std::vector<i32> indexList(n);
    std::iota(indexList.begin(), indexList.end(), 0);

    // Check winding order (ensure counter-clockwise)
    f32 signedArea = 0.0f;
    for (i32 i = 0; i < n; ++i) {
        i32 j = (i + 1) % n;
        signedArea += contour[i].x * contour[j].y;
        signedArea -= contour[j].x * contour[i].y;
    }
    bool isCCW = signedArea > 0.0f;

    // Make a mutable copy of the contour
    std::vector<Math::Vector2> poly = contour;
    if (!isCCW) {
        std::reverse(poly.begin(), poly.end());
        // Reverse index mapping too
        std::reverse(indexList.begin(), indexList.end());
    }

    // Ear clipping
    std::vector<i32> remaining(n);
    std::iota(remaining.begin(), remaining.end(), 0);

    i32 maxIterations = n * n;  // Safety limit
    i32 iteration = 0;

    while (remaining.size() > 2 && iteration < maxIterations) {
        bool earFound = false;
        i32 rn = static_cast<i32>(remaining.size());

        for (i32 i = 0; i < rn; ++i) {
            i32 prev = remaining[(i + rn - 1) % rn];
            i32 curr = remaining[i];
            i32 next = remaining[(i + 1) % rn];

            if (IsEar(poly, prev, curr, next)) {
                indices.push_back(static_cast<u32>(indexList[prev]));
                indices.push_back(static_cast<u32>(indexList[curr]));
                indices.push_back(static_cast<u32>(indexList[next]));

                remaining.erase(remaining.begin() + i);
                earFound = true;
                break;
            }
        }

        if (!earFound) break;  // Degenerate polygon
        ++iteration;
    }

    return indices;
}

// --- Mesh Generation ---

FourierMeshData FourierMeshDecomposition::GenerateMeshFromContour(const std::vector<Math::Vector2>& contour) {
    FourierMeshData mesh;
    if (contour.size() < 3) return mesh;

    // Create vertices in XY plane (Z = 0)
    mesh.vertices.reserve(contour.size());
    for (usize i = 0; i < contour.size(); ++i) {
        FourierMeshData::Vertex v;
        v.position = Math::Vector3(contour[i].x, contour[i].y, 0.0f);
        v.normal = Math::Vector3(0.0f, 0.0f, 1.0f);

        // Compute UV from bounding box
        f32 u = 0.0f, vCoord = 0.0f;
        // Simple planar UV mapping
        v.uv = Math::Vector2(contour[i].x, contour[i].y);
        mesh.vertices.push_back(v);
    }

    // Normalize UVs to [0,1] based on bounding box
    if (!mesh.vertices.empty()) {
        f32 minX = mesh.vertices[0].position.x, maxX = minX;
        f32 minY = mesh.vertices[0].position.y, maxY = minY;
        for (const auto& v : mesh.vertices) {
            minX = Math::Min(minX, v.position.x);
            maxX = Math::Max(maxX, v.position.x);
            minY = Math::Min(minY, v.position.y);
            maxY = Math::Max(maxY, v.position.y);
        }
        f32 rangeX = maxX - minX;
        f32 rangeY = maxY - minY;
        if (rangeX < Math::EPSILON) rangeX = 1.0f;
        if (rangeY < Math::EPSILON) rangeY = 1.0f;
        for (auto& v : mesh.vertices) {
            v.uv.x = (v.position.x - minX) / rangeX;
            v.uv.y = (v.position.y - minY) / rangeY;
        }
    }

    // Triangulate
    mesh.indices = TriangulateContour(contour);

    return mesh;
}

FourierMeshData FourierMeshDecomposition::GenerateExtrudedMesh(const std::vector<Math::Vector2>& contour, f32 extrudeDepth) {
    FourierMeshData mesh;
    i32 n = static_cast<i32>(contour.size());
    if (n < 3) return mesh;

    f32 halfDepth = extrudeDepth * 0.5f;

    // --- Front cap (Z = +halfDepth) ---
    u32 frontStart = 0;
    for (i32 i = 0; i < n; ++i) {
        FourierMeshData::Vertex v;
        v.position = Math::Vector3(contour[i].x, contour[i].y, halfDepth);
        v.normal = Math::Vector3(0.0f, 0.0f, 1.0f);
        v.uv = Math::Vector2(contour[i].x, contour[i].y);  // Will normalize later
        mesh.vertices.push_back(v);
    }

    // --- Back cap (Z = -halfDepth) ---
    u32 backStart = static_cast<u32>(mesh.vertices.size());
    for (i32 i = 0; i < n; ++i) {
        FourierMeshData::Vertex v;
        v.position = Math::Vector3(contour[i].x, contour[i].y, -halfDepth);
        v.normal = Math::Vector3(0.0f, 0.0f, -1.0f);
        v.uv = Math::Vector2(contour[i].x, contour[i].y);
        mesh.vertices.push_back(v);
    }

    // Triangulate front cap
    auto frontIndices = TriangulateContour(contour);
    for (u32 idx : frontIndices) {
        mesh.indices.push_back(frontStart + idx);
    }

    // Triangulate back cap (reverse winding for outward normals)
    for (usize i = 0; i + 2 < frontIndices.size(); i += 3) {
        mesh.indices.push_back(backStart + frontIndices[i]);
        mesh.indices.push_back(backStart + frontIndices[i + 2]);
        mesh.indices.push_back(backStart + frontIndices[i + 1]);
    }

    // --- Side faces ---
    u32 sideStart = static_cast<u32>(mesh.vertices.size());
    for (i32 i = 0; i < n; ++i) {
        i32 next = (i + 1) % n;

        Math::Vector2 edge(contour[next].x - contour[i].x,
                           contour[next].y - contour[i].y);
        // Outward normal in 2D: perpendicular to edge
        Math::Vector3 sideNormal(edge.y, -edge.x, 0.0f);
        sideNormal.Normalize();

        f32 uCoord = static_cast<f32>(i) / static_cast<f32>(n);
        f32 uNext = static_cast<f32>(next) / static_cast<f32>(n);

        // Four vertices per side quad
        u32 v0 = static_cast<u32>(mesh.vertices.size());
        FourierMeshData::Vertex sv;

        // Front-current
        sv.position = Math::Vector3(contour[i].x, contour[i].y, halfDepth);
        sv.normal = sideNormal;
        sv.uv = Math::Vector2(uCoord, 1.0f);
        mesh.vertices.push_back(sv);

        // Front-next
        sv.position = Math::Vector3(contour[next].x, contour[next].y, halfDepth);
        sv.normal = sideNormal;
        sv.uv = Math::Vector2(uNext, 1.0f);
        mesh.vertices.push_back(sv);

        // Back-next
        sv.position = Math::Vector3(contour[next].x, contour[next].y, -halfDepth);
        sv.normal = sideNormal;
        sv.uv = Math::Vector2(uNext, 0.0f);
        mesh.vertices.push_back(sv);

        // Back-current
        sv.position = Math::Vector3(contour[i].x, contour[i].y, -halfDepth);
        sv.normal = sideNormal;
        sv.uv = Math::Vector2(uCoord, 0.0f);
        mesh.vertices.push_back(sv);

        // Two triangles per quad
        mesh.indices.push_back(v0);
        mesh.indices.push_back(v0 + 1);
        mesh.indices.push_back(v0 + 2);

        mesh.indices.push_back(v0);
        mesh.indices.push_back(v0 + 2);
        mesh.indices.push_back(v0 + 3);
    }

    // Normalize UVs for cap vertices
    if (!mesh.vertices.empty()) {
        f32 minX = mesh.vertices[0].position.x, maxX = minX;
        f32 minY = mesh.vertices[0].position.y, maxY = minY;
        for (u32 i = 0; i < sideStart; ++i) {
            minX = Math::Min(minX, mesh.vertices[i].position.x);
            maxX = Math::Max(maxX, mesh.vertices[i].position.x);
            minY = Math::Min(minY, mesh.vertices[i].position.y);
            maxY = Math::Max(maxY, mesh.vertices[i].position.y);
        }
        f32 rangeX = maxX - minX;
        f32 rangeY = maxY - minY;
        if (rangeX < Math::EPSILON) rangeX = 1.0f;
        if (rangeY < Math::EPSILON) rangeY = 1.0f;
        for (u32 i = 0; i < sideStart; ++i) {
            mesh.vertices[i].uv.x = (mesh.vertices[i].position.x - minX) / rangeX;
            mesh.vertices[i].uv.y = (mesh.vertices[i].position.y - minY) / rangeY;
        }
    }

    return mesh;
}

} // namespace Effects
} // namespace Enjin
