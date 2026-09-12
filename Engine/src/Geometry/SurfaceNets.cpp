#include "Enjin/Geometry/SurfaceNets.h"

#include <algorithm>
#include <cmath>

namespace Enjin {
namespace Geometry {

namespace {

// The eight corners of a cell, in the order the edge list below indexes them.
constexpr i32 kCorner[8][3] = {
    {0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {1, 1, 0},
    {0, 0, 1}, {1, 0, 1}, {0, 1, 1}, {1, 1, 1},
};

// The twelve edges, as pairs of corner indices. Only used to find crossings --
// there is no configuration table, which is the whole point of this mesher.
constexpr i32 kEdge[12][2] = {
    {0, 1}, {2, 3}, {4, 5}, {6, 7},   // along x
    {0, 2}, {1, 3}, {4, 6}, {5, 7},   // along y
    {0, 4}, {1, 5}, {2, 6}, {3, 7},   // along z
};

constexpr u32 kNoVertex = 0xFFFFFFFFu;

} // namespace

SurfaceMesh BuildSurfaceNet(const ScalarGrid& grid, f32 isoLevel) {
    SurfaceMesh mesh;
    if (grid.dimX < 2 || grid.dimY < 2 || grid.dimZ < 2) return mesh;
    if (grid.values.size() != grid.Count()) return mesh;

    // The cell lattice runs ONE CELL PAST the samples on every side.
    //
    // A cell's corners are samples, so a lattice that stopped at the last
    // sample would never contain the step from the edge of the volume to the
    // air outside it -- and a volume that is solid at its own boundary would
    // mesh to nothing at all. That is not a small gap: it is an open shell, a
    // collider with an inside you fall out through, and it is what the first
    // version of this did.
    //
    // Cell (i,j,k) here has its minimum corner at SAMPLE (i-1, j-1, k-1).
    // ScalarGrid::At reads anything outside as air, so the outermost ring of
    // cells is exactly the transition that seals the surface.
    const i32 sx = static_cast<i32>(grid.dimX);
    const i32 sy = static_cast<i32>(grid.dimY);
    const i32 sz = static_cast<i32>(grid.dimZ);
    const i32 lx = sx + 1, ly = sy + 1, lz = sz + 1;   // cells per axis

    std::vector<u32> cellVertex(static_cast<usize>(lx) * ly * lz, kNoVertex);
    auto cellAt = [&](i32 i, i32 j, i32 k) -> u32& {
        return cellVertex[(static_cast<usize>(k) * ly + j) * lx + i];
    };
    auto sample = [&](i32 x, i32 y, i32 z) {
        if (x < 0 || y < 0 || z < 0 || x >= sx || y >= sy || z >= sz) return 1.0f;
        return grid.values[grid.Index(static_cast<u32>(x), static_cast<u32>(y),
                                      static_cast<u32>(z))] - isoLevel;
    };
    auto solidAt = [&](i32 x, i32 y, i32 z) { return sample(x, y, z) < 0.0f; };

    // --- pass one: a vertex in every cell the surface passes through ---------
    //
    // Placed at the average of the crossings on that cell's edges, which is
    // what makes the surface land on the field rather than on the grid. A
    // vertex at the cell centre instead would give blocks.
    for (i32 k = 0; k < lz; ++k) {
        for (i32 j = 0; j < ly; ++j) {
            for (i32 i = 0; i < lx; ++i) {
                const i32 bx = i - 1, by = j - 1, bz = k - 1;

                f32 corner[8];
                u32 mask = 0;
                for (u32 ci = 0; ci < 8; ++ci) {
                    corner[ci] = sample(bx + kCorner[ci][0], by + kCorner[ci][1],
                                        bz + kCorner[ci][2]);
                    if (corner[ci] < 0.0f) mask |= (1u << ci);
                }
                // All solid or all air: no surface here.
                if (mask == 0 || mask == 0xFFu) continue;

                f32 sumX = 0.0f, sumY = 0.0f, sumZ = 0.0f;
                u32 crossings = 0;
                for (const auto& e : kEdge) {
                    const f32 a = corner[e[0]];
                    const f32 b = corner[e[1]];
                    if ((a < 0.0f) == (b < 0.0f)) continue;

                    // Where along the edge the field actually reaches zero.
                    // Guarded because two equal values would divide by nothing,
                    // and one NaN vertex poisons the whole mesh rather than one
                    // triangle.
                    const f32 denom = a - b;
                    f32 t = (std::fabs(denom) > 1e-12f) ? (a / denom) : 0.5f;
                    t = std::max(0.0f, std::min(1.0f, t));

                    const f32 ax = static_cast<f32>(kCorner[e[0]][0]);
                    const f32 ay = static_cast<f32>(kCorner[e[0]][1]);
                    const f32 az = static_cast<f32>(kCorner[e[0]][2]);
                    sumX += ax + (static_cast<f32>(kCorner[e[1]][0]) - ax) * t;
                    sumY += ay + (static_cast<f32>(kCorner[e[1]][1]) - ay) * t;
                    sumZ += az + (static_cast<f32>(kCorner[e[1]][2]) - az) * t;
                    ++crossings;
                }
                if (crossings == 0) continue;

                const f32 inv = 1.0f / static_cast<f32>(crossings);

                // The gradient of the field is the surface normal, by central
                // difference around the cell. Taken from the FIELD rather than
                // averaged from triangles, because face-averaged normals shade
                // a smooth cave wall as facets.
                const f32 gx = (sample(bx + 1, by, bz) - sample(bx - 1, by, bz)) +
                               (sample(bx + 2, by, bz) - sample(bx, by, bz));
                const f32 gy = (sample(bx, by + 1, bz) - sample(bx, by - 1, bz)) +
                               (sample(bx, by + 2, bz) - sample(bx, by, bz));
                const f32 gz = (sample(bx, by, bz + 1) - sample(bx, by, bz - 1)) +
                               (sample(bx, by, bz + 2) - sample(bx, by, bz));
                const f32 glen = std::sqrt(gx * gx + gy * gy + gz * gz);
                const Math::Vector3 n = (glen > 1e-8f)
                                            ? Math::Vector3(gx / glen, gy / glen, gz / glen)
                                            : Math::Vector3(0.0f, 1.0f, 0.0f);

                cellAt(i, j, k) = static_cast<u32>(mesh.positions.size());
                mesh.positions.push_back(Math::Vector3(
                    grid.origin.x + (static_cast<f32>(bx) + sumX * inv) * grid.voxelSize,
                    grid.origin.y + (static_cast<f32>(by) + sumY * inv) * grid.voxelSize,
                    grid.origin.z + (static_cast<f32>(bz) + sumZ * inv) * grid.voxelSize));
                mesh.normals.push_back(n);
            }
        }
    }

    if (mesh.positions.empty()) return mesh;

    // --- pass two: a quad across every sample edge the surface crosses -------
    //
    // The dual of pass one. Each edge between two samples is shared by exactly
    // four cells; if the field changes sign along it, those four cells each
    // hold a vertex and together they make one quad. That is what makes the
    // result closed without any configuration table.
    //
    // Winding follows the sign, so the face points from solid towards air. Get
    // this backwards and a cave is inside out -- which reads as a lighting or
    // material bug rather than a mesher one.
    auto quad = [&](u32 a, u32 b, u32 c, u32 d, bool flip) {
        if (a == kNoVertex || b == kNoVertex || c == kNoVertex || d == kNoVertex) return;
        if (!flip) {
            mesh.indices.push_back(a); mesh.indices.push_back(b); mesh.indices.push_back(c);
            mesh.indices.push_back(a); mesh.indices.push_back(c); mesh.indices.push_back(d);
        } else {
            mesh.indices.push_back(a); mesh.indices.push_back(c); mesh.indices.push_back(b);
            mesh.indices.push_back(a); mesh.indices.push_back(d); mesh.indices.push_back(c);
        }
    };

    // Edges start one before the first sample so the boundary faces exist.
    for (i32 z = 0; z < sz; ++z) {
        for (i32 y = 0; y < sy; ++y) {
            for (i32 x = -1; x < sx; ++x) {
                const bool a = solidAt(x, y, z);
                const bool b = solidAt(x + 1, y, z);
                if (a == b) continue;
                quad(cellAt(x + 1, y, z), cellAt(x + 1, y + 1, z),
                     cellAt(x + 1, y + 1, z + 1), cellAt(x + 1, y, z + 1), a);
            }
        }
    }
    for (i32 z = 0; z < sz; ++z) {
        for (i32 y = -1; y < sy; ++y) {
            for (i32 x = 0; x < sx; ++x) {
                const bool a = solidAt(x, y, z);
                const bool b = solidAt(x, y + 1, z);
                if (a == b) continue;
                quad(cellAt(x, y + 1, z), cellAt(x + 1, y + 1, z),
                     cellAt(x + 1, y + 1, z + 1), cellAt(x, y + 1, z + 1), !a);
            }
        }
    }
    for (i32 z = -1; z < sz; ++z) {
        for (i32 y = 0; y < sy; ++y) {
            for (i32 x = 0; x < sx; ++x) {
                const bool a = solidAt(x, y, z);
                const bool b = solidAt(x, y, z + 1);
                if (a == b) continue;
                quad(cellAt(x, y, z + 1), cellAt(x + 1, y, z + 1),
                     cellAt(x + 1, y + 1, z + 1), cellAt(x, y + 1, z + 1), a);
            }
        }
    }

    return mesh;
}

void SampleField(ScalarGrid& grid, const std::function<f32(const Math::Vector3&)>& field) {
    if (!field) return;
    grid.values.assign(grid.Count(), 1.0f);
    for (u32 z = 0; z < grid.dimZ; ++z) {
        for (u32 y = 0; y < grid.dimY; ++y) {
            for (u32 x = 0; x < grid.dimX; ++x) {
                grid.values[grid.Index(x, y, z)] = field(grid.PositionOf(x, y, z));
            }
        }
    }
}

} // namespace Geometry
} // namespace Enjin
