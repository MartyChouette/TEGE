#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Math.h"
#include <vector>
#include <utility>

/**
 * @file Projection4D.h
 * @brief Stereographic and perspective projection of 4D polytopes
 *
 * Provides tools for constructing, rotating, and projecting 4D geometry
 * (tesseract, 5-cell, 16-cell, 24-cell, 120-cell) down to 3D for visualization.
 * Supports stereographic and perspective projection, 4D rotation in all 6 planes,
 * and wireframe mesh generation for rendering.
 */

namespace Enjin {
namespace Effects {

// A point in 4-dimensional space
struct Vector4D {
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 z = 0.0f;
    f32 w = 0.0f;

    constexpr Vector4D() = default;
    constexpr Vector4D(f32 x, f32 y, f32 z, f32 w) : x(x), y(y), z(z), w(w) {}

    constexpr Vector4D operator+(const Vector4D& o) const { return {x + o.x, y + o.y, z + o.z, w + o.w}; }
    constexpr Vector4D operator-(const Vector4D& o) const { return {x - o.x, y - o.y, z - o.z, w - o.w}; }
    constexpr Vector4D operator*(f32 s) const { return {x * s, y * s, z * s, w * s}; }
    constexpr Vector4D operator/(f32 s) const { return {x / s, y / s, z / s, w / s}; }

    f32 Length() const { return Math::Sqrt(x * x + y * y + z * z + w * w); }
    Vector4D Normalized() const {
        f32 len = Length();
        return len > Math::EPSILON ? *this / len : Vector4D{};
    }
    constexpr f32 Dot(const Vector4D& o) const { return x * o.x + y * o.y + z * o.z + w * o.w; }
};

// 4x4 rotation matrix operating in 4D space
// (Not to be confused with Math::Matrix4 which is a 3D homogeneous matrix)
struct Matrix4D {
    f32 m[4][4] = {};

    Matrix4D() {
        // Identity
        m[0][0] = m[1][1] = m[2][2] = m[3][3] = 1.0f;
    }

    Vector4D Transform(const Vector4D& v) const {
        return {
            m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z + m[0][3] * v.w,
            m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z + m[1][3] * v.w,
            m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z + m[2][3] * v.w,
            m[3][0] * v.x + m[3][1] * v.y + m[3][2] * v.z + m[3][3] * v.w
        };
    }

    Matrix4D operator*(const Matrix4D& o) const {
        Matrix4D result;
        for (i32 i = 0; i < 4; ++i) {
            for (i32 j = 0; j < 4; ++j) {
                result.m[i][j] = 0.0f;
                for (i32 k = 0; k < 4; ++k) {
                    result.m[i][j] += m[i][k] * o.m[k][j];
                }
            }
        }
        return result;
    }

    // --- 4D rotation matrices (6 planes of rotation) ---

    static Matrix4D RotateXY(f32 angle);   // Rotation in XY plane
    static Matrix4D RotateXZ(f32 angle);   // Rotation in XZ plane
    static Matrix4D RotateXW(f32 angle);   // Rotation in XW plane
    static Matrix4D RotateYZ(f32 angle);   // Rotation in YZ plane
    static Matrix4D RotateYW(f32 angle);   // Rotation in YW plane
    static Matrix4D RotateZW(f32 angle);   // Rotation in ZW plane
};

// A 4D polytope: vertices + edge connectivity + optional face data
struct Polytope4D {
    std::vector<Vector4D> vertices;
    std::vector<std::pair<u32, u32>> edges;

    // Optional face data (triangulated): each face is a list of vertex indices
    struct Face {
        std::vector<u32> vertexIndices;
    };
    std::vector<Face> faces;
};

// Configuration for animated 4D rotation
struct RotationConfig4D {
    f32 speedXY = 0.0f;  // Radians per second in XY plane
    f32 speedXZ = 0.0f;
    f32 speedXW = 0.5f;  // Default: gentle XW rotation
    f32 speedYZ = 0.0f;
    f32 speedYW = 0.3f;  // Default: gentle YW rotation
    f32 speedZW = 0.0f;
};

// Simple mesh data output for wireframe tube rendering
struct Projection4DMeshData {
    struct Vertex {
        Math::Vector3 position;
        Math::Vector3 normal;
        Math::Vector2 uv;
    };

    std::vector<Vertex> vertices;
    std::vector<u32> indices;
};

/**
 * @brief 4D polytope projection and visualization
 *
 * Constructs canonical 4D polytopes, applies 4D rotations, and projects
 * them to 3D via stereographic or perspective projection. Generates
 * renderable wireframe tube meshes from the projected edges.
 */
class ENJIN_API Projection4D {
public:
    Projection4D() = default;
    ~Projection4D() = default;

    // --- Projection Methods ---

    /**
     * @brief Stereographic projection from 4D to 3D
     *
     * Projects from the 4D unit sphere through the south pole.
     * Points near w = -projectionDistance map to infinity.
     *
     * @param point 4D point to project
     * @param projectionDistance Distance of the projection center (default 2.0)
     * @return 3D projected point
     */
    static Math::Vector3 StereographicProject(const Vector4D& point, f32 projectionDistance = 2.0f);

    /**
     * @brief Perspective projection from 4D to 3D
     *
     * Simulates a 4D "camera" looking along the W axis.
     *
     * @param point 4D point to project
     * @param fov4D Field of view in the 4th dimension (in radians)
     * @return 3D projected point
     */
    static Math::Vector3 PerspectiveProject4D(const Vector4D& point, f32 fov4D = 1.5f);

    // --- Built-in Polytopes ---

    /**
     * @brief Generate a tesseract (4D hypercube)
     * 16 vertices, 32 edges
     */
    static Polytope4D GenerateTesseract();

    /**
     * @brief Generate a 5-cell (4D simplex / pentachoron)
     * 5 vertices, 10 edges
     */
    static Polytope4D Generate5Cell();

    /**
     * @brief Generate a 16-cell (4D cross-polytope / hexadecachoron)
     * 8 vertices, 24 edges
     */
    static Polytope4D Generate16Cell();

    /**
     * @brief Generate a 24-cell (icositetrachoron)
     * 24 vertices, 96 edges
     */
    static Polytope4D Generate24Cell();

    /**
     * @brief Generate a 120-cell (hecatonicosachoron)
     * 600 vertices, 1200 edges
     */
    static Polytope4D Generate120Cell();

    // --- Rotation & Animation ---

    /**
     * @brief Build the combined rotation matrix from elapsed time and config
     *
     * @param dt Delta time in seconds since last call
     * @param config Rotation speeds for each plane
     * @return Combined 4D rotation matrix
     */
    static Matrix4D BuildRotationMatrix(f32 dt, const RotationConfig4D& config);

    /**
     * @brief Apply a rotation to all vertices of a polytope
     *
     * @param polytope The polytope to transform (modified in place)
     * @param rotation The 4D rotation matrix
     */
    static void ApplyRotation(Polytope4D& polytope, const Matrix4D& rotation);

    /**
     * @brief Animate rotation: accumulates rotation angles and applies to polytope
     *
     * @param polytope The polytope to rotate (modified in place)
     * @param dt Delta time in seconds
     * @param config Rotation speed configuration
     */
    void AnimateRotation(Polytope4D& polytope, f32 dt, const RotationConfig4D& config);

    // --- Mesh Generation ---

    /**
     * @brief Generate a wireframe tube mesh from a projected polytope
     *
     * Creates cylindrical tube segments along each edge of the polytope,
     * projected to 3D. Suitable for rendering with the standard mesh pipeline.
     *
     * @param polytope The 4D polytope (vertices will be projected)
     * @param lineWidth Radius of the tube segments
     * @param projectionDistance Stereographic projection distance
     * @param tubeSegments Number of segments around each tube (default 8)
     * @return Renderable mesh data
     */
    static Projection4DMeshData GenerateWireframeMesh(
        const Polytope4D& polytope,
        f32 lineWidth = 0.02f,
        f32 projectionDistance = 2.0f,
        u32 tubeSegments = 8
    );

    /**
     * @brief Get current accumulated rotation angles (in radians)
     */
    f32 GetAngleXY() const { return m_AngleXY; }
    f32 GetAngleXZ() const { return m_AngleXZ; }
    f32 GetAngleXW() const { return m_AngleXW; }
    f32 GetAngleYZ() const { return m_AngleYZ; }
    f32 GetAngleYW() const { return m_AngleYW; }
    f32 GetAngleZW() const { return m_AngleZW; }

    void ResetAngles();

private:
    // Accumulated rotation angles for AnimateRotation
    f32 m_AngleXY = 0.0f;
    f32 m_AngleXZ = 0.0f;
    f32 m_AngleXW = 0.0f;
    f32 m_AngleYZ = 0.0f;
    f32 m_AngleYW = 0.0f;
    f32 m_AngleZW = 0.0f;
};

} // namespace Effects
} // namespace Enjin
