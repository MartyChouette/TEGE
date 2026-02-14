#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Math.h"
#include <vector>
#include <cmath>

/**
 * @file FourierMesh.h
 * @brief Fourier Transform mesh decomposition and reconstruction
 *
 * Takes a 2D contour (silhouette) and computes its Discrete Fourier Transform.
 * The resulting coefficients can reconstruct the contour with varying levels
 * of detail, animate the reconstruction by adding terms over time, and
 * generate 3D meshes from the reconstructed contour via triangulation and
 * optional extrusion.
 */

namespace Enjin {
namespace Effects {

// A single Fourier coefficient in complex form
struct FourierCoefficient {
    f32 real = 0.0f;         // Real part of the complex coefficient
    f32 imag = 0.0f;         // Imaginary part of the complex coefficient
    i32 frequency = 0;       // Frequency index k
    f32 amplitude = 0.0f;    // |c_k| = sqrt(real^2 + imag^2)
    f32 phase = 0.0f;        // atan2(imag, real)
};

// Simple mesh data output (vertices + indices)
struct FourierMeshData {
    struct Vertex {
        Math::Vector3 position;
        Math::Vector3 normal;
        Math::Vector2 uv;
    };

    std::vector<Vertex> vertices;
    std::vector<u32> indices;
};

/**
 * @brief Discrete Fourier Transform mesh decomposition
 *
 * Computes the DFT of a 2D contour and allows:
 * - Reconstruction at arbitrary parameter t with configurable term count
 * - Animated reconstruction by slowly increasing term count
 * - Generation of 2D triangulated mesh or 3D extruded mesh from contour
 */
class ENJIN_API FourierMeshDecomposition {
public:
    FourierMeshDecomposition() = default;
    ~FourierMeshDecomposition() = default;

    /**
     * @brief Decompose a 2D contour into Fourier coefficients
     *
     * Computes c_k = (1/N) * sum_{n=0}^{N-1} point_n * exp(-2*pi*i*k*n/N)
     * for frequencies from -numCoefficients/2 to +numCoefficients/2.
     *
     * @param contour Input contour points (closed curve, last point connects to first)
     * @param numCoefficients Number of Fourier coefficients to compute (0 = use all N)
     */
    void Decompose(const std::vector<Math::Vector2>& contour, i32 numCoefficients = 0);

    /**
     * @brief Evaluate the Fourier series at parameter t
     *
     * Returns the point on the reconstructed contour at parameter t in [0, 1).
     * Uses the first numTerms coefficients sorted by amplitude.
     *
     * @param t Parameter along the curve (0 = start, 1 = full cycle)
     * @param numTerms Number of Fourier terms to include (0 = use all)
     * @return Reconstructed 2D point
     */
    Math::Vector2 Reconstruct(f32 t, i32 numTerms = 0) const;

    /**
     * @brief Generate a full contour approximation
     *
     * Evaluates the Fourier series at numSamples evenly-spaced parameter values.
     *
     * @param numTerms Number of Fourier terms to use
     * @param numSamples Number of sample points around the curve
     * @return Vector of 2D points forming the reconstructed contour
     */
    std::vector<Math::Vector2> GenerateApproximation(i32 numTerms, i32 numSamples = 256) const;

    /**
     * @brief Get current animation state for term-by-term reconstruction
     *
     * Smoothly animates the reconstruction by slowly increasing the number
     * of visible terms over time.
     *
     * @param time Current time in seconds
     * @param maxTerms Maximum number of terms to reach
     * @return Number of terms to use at this time (fractional for interpolation)
     */
    f32 Animate(f32 time, i32 maxTerms) const;

    /**
     * @brief Generate an animated reconstruction contour at a given time
     *
     * @param time Current time in seconds
     * @param maxTerms Maximum terms for the animation
     * @param numSamples Number of sample points
     * @return Contour points for the current animation frame
     */
    std::vector<Math::Vector2> GenerateAnimatedContour(f32 time, i32 maxTerms, i32 numSamples = 256) const;

    /**
     * @brief Generate a 2D triangulated mesh from a contour
     *
     * Uses ear-clipping triangulation to create a filled mesh from the contour.
     *
     * @param contour 2D contour points
     * @return Mesh data with vertices in XY plane (Z=0) and triangle indices
     */
    static FourierMeshData GenerateMeshFromContour(const std::vector<Math::Vector2>& contour);

    /**
     * @brief Generate a 3D extruded mesh from a contour
     *
     * Triangulates the contour for front/back caps and creates side faces
     * by connecting front and back contour edges.
     *
     * @param contour 2D contour points
     * @param extrudeDepth Depth of extrusion along Z axis
     * @return Mesh data with 3D vertices and triangle indices
     */
    static FourierMeshData GenerateExtrudedMesh(const std::vector<Math::Vector2>& contour, f32 extrudeDepth);

    // --- Accessors ---

    const std::vector<FourierCoefficient>& GetCoefficients() const { return m_Coefficients; }
    i32 GetCoefficientCount() const { return static_cast<i32>(m_Coefficients.size()); }
    bool HasData() const { return !m_Coefficients.empty(); }

    /**
     * @brief Get the original contour that was decomposed
     */
    const std::vector<Math::Vector2>& GetOriginalContour() const { return m_OriginalContour; }

private:
    std::vector<FourierCoefficient> m_Coefficients;  // Sorted by amplitude (descending)
    std::vector<Math::Vector2> m_OriginalContour;

    // Ear-clipping triangulation helper
    static std::vector<u32> TriangulateContour(const std::vector<Math::Vector2>& contour);
    static bool IsEar(const std::vector<Math::Vector2>& polygon, i32 prev, i32 curr, i32 next);
    static bool PointInTriangle(const Math::Vector2& p, const Math::Vector2& a,
                                const Math::Vector2& b, const Math::Vector2& c);
    static f32 Cross2D(const Math::Vector2& a, const Math::Vector2& b);
};

} // namespace Effects
} // namespace Enjin
