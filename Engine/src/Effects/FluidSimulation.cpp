#include "Enjin/Effects/FluidSimulation.h"
#include "Enjin/ECS/Components/FluidVolume.h"
#include "Enjin/ECS/Components/Transform.h"
#include <algorithm>
#include <cmath>

namespace Enjin {
namespace Effects {

void FluidSimulation::Update(f32 dt, ECS::World* world) {
    if (!world || dt <= 0.0f) return;
    if (dt > 0.5f) return;  // Skip on long frames (loading/pause)
    dt = std::min(dt, 1.0f / 30.0f);  // Clamp for stability

    for (ECS::Entity entity : world->GetEntitiesWithComponent<ECS::FluidVolumeComponent>()) {
        auto* vol = world->GetComponent<ECS::FluidVolumeComponent>(entity);
        if (!vol || !vol->isActive) continue;

        // Initialize grid if needed
        auto& grid = m_Grids[entity];
        bool is3D = vol->dimension == ECS::FluidDimension::Mode3D;
        u32 clampedSize = vol->gridSize;
        if (is3D) clampedSize = std::min(clampedSize, 48u);
        else clampedSize = std::min(clampedSize, 128u);
        clampedSize = std::max(clampedSize, 8u);

        // Hysteresis: only reallocate if dimension mode changed or size differs by >10%
        bool needRealloc = (grid.is3D != is3D) || (grid.N == 0);
        if (!needRealloc && grid.N != clampedSize) {
            f32 ratio = static_cast<f32>(clampedSize) / static_cast<f32>(grid.N);
            needRealloc = (ratio > 1.1f || ratio < 0.9f);
        }
        if (needRealloc) {
            grid.Allocate(clampedSize, is3D);
            vol->simulationInitialized = true;
        }

        i32 iterations = vol->solverIterations;
        if (is3D && clampedSize > 32) iterations = std::min(iterations, 10);

        if (is3D) {
            Step3D(grid, dt, vol->viscosity, vol->diffusion, vol->dissipation,
                   vol->velocityDissipation, iterations, vol->buoyancy);
        } else {
            Step2D(grid, dt, vol->viscosity, vol->diffusion, vol->dissipation,
                   vol->velocityDissipation, iterations, vol->buoyancy);
        }
    }
}

void FluidSimulation::AddDensityAtWorldPos(ECS::Entity entity, const Math::Vector3& worldPos, f32 amount, f32 radius) {
    auto it = m_Grids.find(entity);
    if (it == m_Grids.end()) return;
    auto& grid = it->second;
    u32 N = grid.N;

    // World pos needs to be converted to grid coords by the caller or we do a simple center-based mapping
    // For now, treat worldPos as grid-space coordinates normalized to [0,N]
    if (!grid.is3D) {
        i32 cx = static_cast<i32>(worldPos.x);
        i32 cy = static_cast<i32>(worldPos.y);
        i32 r = static_cast<i32>(radius);
        for (i32 j = std::max(1, cy - r); j <= std::min(static_cast<i32>(N), cy + r); ++j) {
            for (i32 i = std::max(1, cx - r); i <= std::min(static_cast<i32>(N), cx + r); ++i) {
                f32 dx = static_cast<f32>(i - cx);
                f32 dy = static_cast<f32>(j - cy);
                f32 dist = std::sqrt(dx * dx + dy * dy);
                if (dist <= radius) {
                    f32 falloff = 1.0f - dist / radius;
                    grid.density[grid.IX(i, j)] += amount * falloff;
                }
            }
        }
    } else {
        i32 cx = static_cast<i32>(worldPos.x);
        i32 cy = static_cast<i32>(worldPos.y);
        i32 cz = static_cast<i32>(worldPos.z);
        i32 r = static_cast<i32>(radius);
        for (i32 k = std::max(1, cz - r); k <= std::min(static_cast<i32>(N), cz + r); ++k) {
            for (i32 j = std::max(1, cy - r); j <= std::min(static_cast<i32>(N), cy + r); ++j) {
                for (i32 i = std::max(1, cx - r); i <= std::min(static_cast<i32>(N), cx + r); ++i) {
                    f32 dx = static_cast<f32>(i - cx);
                    f32 dy = static_cast<f32>(j - cy);
                    f32 dz = static_cast<f32>(k - cz);
                    f32 dist = std::sqrt(dx * dx + dy * dy + dz * dz);
                    if (dist <= radius) {
                        f32 falloff = 1.0f - dist / radius;
                        grid.density[grid.IX3(i, j, k)] += amount * falloff;
                    }
                }
            }
        }
    }
}

void FluidSimulation::AddVelocityAtWorldPos(ECS::Entity entity, const Math::Vector3& worldPos, const Math::Vector3& velocity, f32 radius) {
    auto it = m_Grids.find(entity);
    if (it == m_Grids.end()) return;
    auto& grid = it->second;
    u32 N = grid.N;

    if (!grid.is3D) {
        i32 cx = static_cast<i32>(worldPos.x);
        i32 cy = static_cast<i32>(worldPos.y);
        i32 r = static_cast<i32>(radius);
        for (i32 j = std::max(1, cy - r); j <= std::min(static_cast<i32>(N), cy + r); ++j) {
            for (i32 i = std::max(1, cx - r); i <= std::min(static_cast<i32>(N), cx + r); ++i) {
                f32 dx = static_cast<f32>(i - cx);
                f32 dy = static_cast<f32>(j - cy);
                f32 dist = std::sqrt(dx * dx + dy * dy);
                if (dist <= radius) {
                    f32 falloff = 1.0f - dist / radius;
                    grid.velocityX[grid.IX(i, j)] += velocity.x * falloff;
                    grid.velocityY[grid.IX(i, j)] += velocity.y * falloff;
                }
            }
        }
    } else {
        i32 cx = static_cast<i32>(worldPos.x);
        i32 cy = static_cast<i32>(worldPos.y);
        i32 cz = static_cast<i32>(worldPos.z);
        i32 r = static_cast<i32>(radius);
        for (i32 k = std::max(1, cz - r); k <= std::min(static_cast<i32>(N), cz + r); ++k) {
            for (i32 j = std::max(1, cy - r); j <= std::min(static_cast<i32>(N), cy + r); ++j) {
                for (i32 i = std::max(1, cx - r); i <= std::min(static_cast<i32>(N), cx + r); ++i) {
                    f32 dx = static_cast<f32>(i - cx);
                    f32 dy = static_cast<f32>(j - cy);
                    f32 dz = static_cast<f32>(k - cz);
                    f32 dist = std::sqrt(dx * dx + dy * dy + dz * dz);
                    if (dist <= radius) {
                        f32 falloff = 1.0f - dist / radius;
                        grid.velocityX[grid.IX3(i, j, k)] += velocity.x * falloff;
                        grid.velocityY[grid.IX3(i, j, k)] += velocity.y * falloff;
                        grid.velocityZ[grid.IX3(i, j, k)] += velocity.z * falloff;
                    }
                }
            }
        }
    }
}

void FluidSimulation::Reset(ECS::Entity entity) {
    auto it = m_Grids.find(entity);
    if (it != m_Grids.end()) {
        it->second.Clear();
    }
}

void FluidSimulation::OnEntityRemoved(ECS::Entity entity) {
    m_Grids.erase(entity);
}

const FluidGridData* FluidSimulation::GetGridData(ECS::Entity entity) const {
    auto it = m_Grids.find(entity);
    return it != m_Grids.end() ? &it->second : nullptr;
}

FluidGridData* FluidSimulation::GetMutableGridData(ECS::Entity entity) {
    auto it = m_Grids.find(entity);
    return it != m_Grids.end() ? &it->second : nullptr;
}

// ============================================================================
// 2D Stable Fluids
// ============================================================================

void FluidSimulation::Step2D(FluidGridData& grid, f32 dt, f32 visc, f32 diff,
                              f32 dissipation, f32 velDissipation, i32 iterations, f32 buoyancy) {
    u32 N = grid.N;
    u32 size = (N + 2) * (N + 2);

    // Apply velocity dissipation to previous buffers
    for (u32 i = 0; i < size; ++i) {
        grid.velocityXPrev[i] = grid.velocityX[i] * velDissipation;
        grid.velocityYPrev[i] = grid.velocityY[i] * velDissipation;
        grid.densityPrev[i] = grid.density[i] * dissipation;
    }

    // Velocity step
    Diffuse2D(grid, 1, grid.velocityX, grid.velocityXPrev, visc, dt, iterations);
    Diffuse2D(grid, 2, grid.velocityY, grid.velocityYPrev, visc, dt, iterations);
    Project2D(grid, grid.velocityX, grid.velocityY, grid.pressure, grid.divergence, iterations);

    std::swap(grid.velocityXPrev, grid.velocityX);
    std::swap(grid.velocityYPrev, grid.velocityY);

    Advect2D(grid, 1, grid.velocityX, grid.velocityXPrev, grid.velocityXPrev, grid.velocityYPrev, dt);
    Advect2D(grid, 2, grid.velocityY, grid.velocityYPrev, grid.velocityXPrev, grid.velocityYPrev, dt);
    Project2D(grid, grid.velocityX, grid.velocityY, grid.pressure, grid.divergence, iterations);

    // Density step
    Diffuse2D(grid, 0, grid.density, grid.densityPrev, diff, dt, iterations);
    std::swap(grid.densityPrev, grid.density);
    Advect2D(grid, 0, grid.density, grid.densityPrev, grid.velocityX, grid.velocityY, dt);

    // Apply buoyancy (upward force proportional to density)
    if (buoyancy > 0.0f) {
        for (u32 j = 1; j <= N; ++j) {
            for (u32 i = 1; i <= N; ++i) {
                grid.velocityY[grid.IX(i, j)] += buoyancy * grid.density[grid.IX(i, j)] * dt;
            }
        }
    }
}

void FluidSimulation::Diffuse2D(FluidGridData& grid, i32 b, std::vector<f32>& x,
                                  const std::vector<f32>& x0, f32 diff, f32 dt, i32 iterations) {
    u32 N = grid.N;
    f32 a = dt * diff * N * N;
    f32 divisor = 1.0f + 4.0f * a;

    for (i32 iter = 0; iter < iterations; ++iter) {
        for (u32 j = 1; j <= N; ++j) {
            for (u32 i = 1; i <= N; ++i) {
                x[grid.IX(i, j)] = (x0[grid.IX(i, j)] +
                    a * (x[grid.IX(i - 1, j)] + x[grid.IX(i + 1, j)] +
                         x[grid.IX(i, j - 1)] + x[grid.IX(i, j + 1)])) / divisor;
            }
        }
        SetBoundary2D(grid, b, x);
    }
}

void FluidSimulation::Advect2D(FluidGridData& grid, i32 b, std::vector<f32>& d,
                                 const std::vector<f32>& d0, const std::vector<f32>& u,
                                 const std::vector<f32>& v, f32 dt) {
    u32 N = grid.N;
    f32 dt0 = dt * N;

    for (u32 j = 1; j <= N; ++j) {
        for (u32 i = 1; i <= N; ++i) {
            // Backtrace
            f32 x = static_cast<f32>(i) - dt0 * u[grid.IX(i, j)];
            f32 y = static_cast<f32>(j) - dt0 * v[grid.IX(i, j)];

            // Clamp to grid
            x = std::max(0.5f, std::min(static_cast<f32>(N) + 0.5f, x));
            y = std::max(0.5f, std::min(static_cast<f32>(N) + 0.5f, y));

            u32 i0 = static_cast<u32>(x);
            u32 i1 = i0 + 1;
            u32 j0 = static_cast<u32>(y);
            u32 j1 = j0 + 1;

            f32 s1 = x - static_cast<f32>(i0);
            f32 s0 = 1.0f - s1;
            f32 t1 = y - static_cast<f32>(j0);
            f32 t0 = 1.0f - t1;

            d[grid.IX(i, j)] = s0 * (t0 * d0[grid.IX(i0, j0)] + t1 * d0[grid.IX(i0, j1)]) +
                               s1 * (t0 * d0[grid.IX(i1, j0)] + t1 * d0[grid.IX(i1, j1)]);
        }
    }
    SetBoundary2D(grid, b, d);
}

void FluidSimulation::Project2D(FluidGridData& grid, std::vector<f32>& u, std::vector<f32>& v,
                                  std::vector<f32>& p, std::vector<f32>& div, i32 iterations) {
    u32 N = grid.N;
    f32 h = 1.0f / N;

    // Compute divergence
    for (u32 j = 1; j <= N; ++j) {
        for (u32 i = 1; i <= N; ++i) {
            div[grid.IX(i, j)] = -0.5f * h * (u[grid.IX(i + 1, j)] - u[grid.IX(i - 1, j)] +
                                                v[grid.IX(i, j + 1)] - v[grid.IX(i, j - 1)]);
            p[grid.IX(i, j)] = 0.0f;
        }
    }
    SetBoundary2D(grid, 0, div);
    SetBoundary2D(grid, 0, p);

    // Solve pressure (Gauss-Seidel)
    for (i32 iter = 0; iter < iterations; ++iter) {
        for (u32 j = 1; j <= N; ++j) {
            for (u32 i = 1; i <= N; ++i) {
                p[grid.IX(i, j)] = (div[grid.IX(i, j)] +
                    p[grid.IX(i - 1, j)] + p[grid.IX(i + 1, j)] +
                    p[grid.IX(i, j - 1)] + p[grid.IX(i, j + 1)]) / 4.0f;
            }
        }
        SetBoundary2D(grid, 0, p);
    }

    // Subtract pressure gradient
    for (u32 j = 1; j <= N; ++j) {
        for (u32 i = 1; i <= N; ++i) {
            u[grid.IX(i, j)] -= 0.5f * N * (p[grid.IX(i + 1, j)] - p[grid.IX(i - 1, j)]);
            v[grid.IX(i, j)] -= 0.5f * N * (p[grid.IX(i, j + 1)] - p[grid.IX(i, j - 1)]);
        }
    }
    SetBoundary2D(grid, 1, u);
    SetBoundary2D(grid, 2, v);
}

void FluidSimulation::SetBoundary2D(FluidGridData& grid, i32 b, std::vector<f32>& x) {
    u32 N = grid.N;
    for (u32 i = 1; i <= N; ++i) {
        x[grid.IX(0, i)]     = b == 1 ? -x[grid.IX(1, i)] : x[grid.IX(1, i)];
        x[grid.IX(N + 1, i)] = b == 1 ? -x[grid.IX(N, i)] : x[grid.IX(N, i)];
        x[grid.IX(i, 0)]     = b == 2 ? -x[grid.IX(i, 1)] : x[grid.IX(i, 1)];
        x[grid.IX(i, N + 1)] = b == 2 ? -x[grid.IX(i, N)] : x[grid.IX(i, N)];
    }
    x[grid.IX(0, 0)]         = 0.5f * (x[grid.IX(1, 0)] + x[grid.IX(0, 1)]);
    x[grid.IX(0, N + 1)]     = 0.5f * (x[grid.IX(1, N + 1)] + x[grid.IX(0, N)]);
    x[grid.IX(N + 1, 0)]     = 0.5f * (x[grid.IX(N, 0)] + x[grid.IX(N + 1, 1)]);
    x[grid.IX(N + 1, N + 1)] = 0.5f * (x[grid.IX(N, N + 1)] + x[grid.IX(N + 1, N)]);
}

// ============================================================================
// 3D Stable Fluids
// ============================================================================

void FluidSimulation::Step3D(FluidGridData& grid, f32 dt, f32 visc, f32 diff,
                              f32 dissipation, f32 velDissipation, i32 iterations, f32 buoyancy) {
    u32 N = grid.N;
    u32 size = (N + 2) * (N + 2) * (N + 2);

    // Apply dissipation
    for (u32 i = 0; i < size; ++i) {
        grid.velocityXPrev[i] = grid.velocityX[i] * velDissipation;
        grid.velocityYPrev[i] = grid.velocityY[i] * velDissipation;
        grid.velocityZPrev[i] = grid.velocityZ[i] * velDissipation;
        grid.densityPrev[i] = grid.density[i] * dissipation;
    }

    // Velocity step
    Diffuse3D(grid, 1, grid.velocityX, grid.velocityXPrev, visc, dt, iterations);
    Diffuse3D(grid, 2, grid.velocityY, grid.velocityYPrev, visc, dt, iterations);
    Diffuse3D(grid, 3, grid.velocityZ, grid.velocityZPrev, visc, dt, iterations);
    Project3D(grid, grid.velocityX, grid.velocityY, grid.velocityZ, grid.pressure, grid.divergence, iterations);

    std::swap(grid.velocityXPrev, grid.velocityX);
    std::swap(grid.velocityYPrev, grid.velocityY);
    std::swap(grid.velocityZPrev, grid.velocityZ);

    Advect3D(grid, 1, grid.velocityX, grid.velocityXPrev, grid.velocityXPrev, grid.velocityYPrev, grid.velocityZPrev, dt);
    Advect3D(grid, 2, grid.velocityY, grid.velocityYPrev, grid.velocityXPrev, grid.velocityYPrev, grid.velocityZPrev, dt);
    Advect3D(grid, 3, grid.velocityZ, grid.velocityZPrev, grid.velocityXPrev, grid.velocityYPrev, grid.velocityZPrev, dt);
    Project3D(grid, grid.velocityX, grid.velocityY, grid.velocityZ, grid.pressure, grid.divergence, iterations);

    // Density step
    Diffuse3D(grid, 0, grid.density, grid.densityPrev, diff, dt, iterations);
    std::swap(grid.densityPrev, grid.density);
    Advect3D(grid, 0, grid.density, grid.densityPrev, grid.velocityX, grid.velocityY, grid.velocityZ, dt);

    // Apply buoyancy (Y-up)
    if (buoyancy > 0.0f) {
        for (u32 k = 1; k <= N; ++k) {
            for (u32 j = 1; j <= N; ++j) {
                for (u32 i = 1; i <= N; ++i) {
                    grid.velocityY[grid.IX3(i, j, k)] += buoyancy * grid.density[grid.IX3(i, j, k)] * dt;
                }
            }
        }
    }
}

void FluidSimulation::Diffuse3D(FluidGridData& grid, i32 b, std::vector<f32>& x,
                                  const std::vector<f32>& x0, f32 diff, f32 dt, i32 iterations) {
    u32 N = grid.N;
    f32 a = dt * diff * N * N;
    f32 divisor = 1.0f + 6.0f * a;

    for (i32 iter = 0; iter < iterations; ++iter) {
        for (u32 k = 1; k <= N; ++k) {
            for (u32 j = 1; j <= N; ++j) {
                for (u32 i = 1; i <= N; ++i) {
                    x[grid.IX3(i, j, k)] = (x0[grid.IX3(i, j, k)] +
                        a * (x[grid.IX3(i - 1, j, k)] + x[grid.IX3(i + 1, j, k)] +
                             x[grid.IX3(i, j - 1, k)] + x[grid.IX3(i, j + 1, k)] +
                             x[grid.IX3(i, j, k - 1)] + x[grid.IX3(i, j, k + 1)])) / divisor;
                }
            }
        }
        SetBoundary3D(grid, b, x);
    }
}

void FluidSimulation::Advect3D(FluidGridData& grid, i32 b, std::vector<f32>& d,
                                 const std::vector<f32>& d0, const std::vector<f32>& u,
                                 const std::vector<f32>& v, const std::vector<f32>& w, f32 dt) {
    u32 N = grid.N;
    f32 dt0 = dt * N;

    for (u32 k = 1; k <= N; ++k) {
        for (u32 j = 1; j <= N; ++j) {
            for (u32 i = 1; i <= N; ++i) {
                f32 x = static_cast<f32>(i) - dt0 * u[grid.IX3(i, j, k)];
                f32 y = static_cast<f32>(j) - dt0 * v[grid.IX3(i, j, k)];
                f32 z = static_cast<f32>(k) - dt0 * w[grid.IX3(i, j, k)];

                x = std::max(0.5f, std::min(static_cast<f32>(N) + 0.5f, x));
                y = std::max(0.5f, std::min(static_cast<f32>(N) + 0.5f, y));
                z = std::max(0.5f, std::min(static_cast<f32>(N) + 0.5f, z));

                u32 i0 = static_cast<u32>(x); u32 i1 = i0 + 1;
                u32 j0 = static_cast<u32>(y); u32 j1 = j0 + 1;
                u32 k0 = static_cast<u32>(z); u32 k1 = k0 + 1;

                f32 s1 = x - static_cast<f32>(i0); f32 s0 = 1.0f - s1;
                f32 t1 = y - static_cast<f32>(j0); f32 t0 = 1.0f - t1;
                f32 r1 = z - static_cast<f32>(k0); f32 r0 = 1.0f - r1;

                d[grid.IX3(i, j, k)] =
                    r0 * (s0 * (t0 * d0[grid.IX3(i0, j0, k0)] + t1 * d0[grid.IX3(i0, j1, k0)]) +
                           s1 * (t0 * d0[grid.IX3(i1, j0, k0)] + t1 * d0[grid.IX3(i1, j1, k0)])) +
                    r1 * (s0 * (t0 * d0[grid.IX3(i0, j0, k1)] + t1 * d0[grid.IX3(i0, j1, k1)]) +
                           s1 * (t0 * d0[grid.IX3(i1, j0, k1)] + t1 * d0[grid.IX3(i1, j1, k1)]));
            }
        }
    }
    SetBoundary3D(grid, b, d);
}

void FluidSimulation::Project3D(FluidGridData& grid, std::vector<f32>& u, std::vector<f32>& v,
                                  std::vector<f32>& w, std::vector<f32>& p, std::vector<f32>& div, i32 iterations) {
    u32 N = grid.N;
    f32 h = 1.0f / N;

    for (u32 k = 1; k <= N; ++k) {
        for (u32 j = 1; j <= N; ++j) {
            for (u32 i = 1; i <= N; ++i) {
                div[grid.IX3(i, j, k)] = -0.5f * h * (
                    u[grid.IX3(i + 1, j, k)] - u[grid.IX3(i - 1, j, k)] +
                    v[grid.IX3(i, j + 1, k)] - v[grid.IX3(i, j - 1, k)] +
                    w[grid.IX3(i, j, k + 1)] - w[grid.IX3(i, j, k - 1)]);
                p[grid.IX3(i, j, k)] = 0.0f;
            }
        }
    }
    SetBoundary3D(grid, 0, div);
    SetBoundary3D(grid, 0, p);

    for (i32 iter = 0; iter < iterations; ++iter) {
        for (u32 k = 1; k <= N; ++k) {
            for (u32 j = 1; j <= N; ++j) {
                for (u32 i = 1; i <= N; ++i) {
                    p[grid.IX3(i, j, k)] = (div[grid.IX3(i, j, k)] +
                        p[grid.IX3(i - 1, j, k)] + p[grid.IX3(i + 1, j, k)] +
                        p[grid.IX3(i, j - 1, k)] + p[grid.IX3(i, j + 1, k)] +
                        p[grid.IX3(i, j, k - 1)] + p[grid.IX3(i, j, k + 1)]) / 6.0f;
                }
            }
        }
        SetBoundary3D(grid, 0, p);
    }

    for (u32 k = 1; k <= N; ++k) {
        for (u32 j = 1; j <= N; ++j) {
            for (u32 i = 1; i <= N; ++i) {
                u[grid.IX3(i, j, k)] -= 0.5f * N * (p[grid.IX3(i + 1, j, k)] - p[grid.IX3(i - 1, j, k)]);
                v[grid.IX3(i, j, k)] -= 0.5f * N * (p[grid.IX3(i, j + 1, k)] - p[grid.IX3(i, j - 1, k)]);
                w[grid.IX3(i, j, k)] -= 0.5f * N * (p[grid.IX3(i, j, k + 1)] - p[grid.IX3(i, j, k - 1)]);
            }
        }
    }
    SetBoundary3D(grid, 1, u);
    SetBoundary3D(grid, 2, v);
    SetBoundary3D(grid, 3, w);
}

void FluidSimulation::SetBoundary3D(FluidGridData& grid, i32 b, std::vector<f32>& x) {
    u32 N = grid.N;

    // Faces
    for (u32 j = 1; j <= N; ++j) {
        for (u32 i = 1; i <= N; ++i) {
            x[grid.IX3(0, i, j)]     = b == 1 ? -x[grid.IX3(1, i, j)] : x[grid.IX3(1, i, j)];
            x[grid.IX3(N + 1, i, j)] = b == 1 ? -x[grid.IX3(N, i, j)] : x[grid.IX3(N, i, j)];
            x[grid.IX3(i, 0, j)]     = b == 2 ? -x[grid.IX3(i, 1, j)] : x[grid.IX3(i, 1, j)];
            x[grid.IX3(i, N + 1, j)] = b == 2 ? -x[grid.IX3(i, N, j)] : x[grid.IX3(i, N, j)];
            x[grid.IX3(i, j, 0)]     = b == 3 ? -x[grid.IX3(i, j, 1)] : x[grid.IX3(i, j, 1)];
            x[grid.IX3(i, j, N + 1)] = b == 3 ? -x[grid.IX3(i, j, N)] : x[grid.IX3(i, j, N)];
        }
    }

    // Edges (average of two adjacent face cells)
    for (u32 i = 1; i <= N; ++i) {
        x[grid.IX3(0, 0, i)]         = 0.5f * (x[grid.IX3(1, 0, i)] + x[grid.IX3(0, 1, i)]);
        x[grid.IX3(0, N + 1, i)]     = 0.5f * (x[grid.IX3(1, N + 1, i)] + x[grid.IX3(0, N, i)]);
        x[grid.IX3(N + 1, 0, i)]     = 0.5f * (x[grid.IX3(N, 0, i)] + x[grid.IX3(N + 1, 1, i)]);
        x[grid.IX3(N + 1, N + 1, i)] = 0.5f * (x[grid.IX3(N, N + 1, i)] + x[grid.IX3(N + 1, N, i)]);

        x[grid.IX3(0, i, 0)]         = 0.5f * (x[grid.IX3(1, i, 0)] + x[grid.IX3(0, i, 1)]);
        x[grid.IX3(0, i, N + 1)]     = 0.5f * (x[grid.IX3(1, i, N + 1)] + x[grid.IX3(0, i, N)]);
        x[grid.IX3(N + 1, i, 0)]     = 0.5f * (x[grid.IX3(N, i, 0)] + x[grid.IX3(N + 1, i, 1)]);
        x[grid.IX3(N + 1, i, N + 1)] = 0.5f * (x[grid.IX3(N, i, N + 1)] + x[grid.IX3(N + 1, i, N)]);

        x[grid.IX3(i, 0, 0)]         = 0.5f * (x[grid.IX3(i, 1, 0)] + x[grid.IX3(i, 0, 1)]);
        x[grid.IX3(i, 0, N + 1)]     = 0.5f * (x[grid.IX3(i, 1, N + 1)] + x[grid.IX3(i, 0, N)]);
        x[grid.IX3(i, N + 1, 0)]     = 0.5f * (x[grid.IX3(i, N, 0)] + x[grid.IX3(i, N + 1, 1)]);
        x[grid.IX3(i, N + 1, N + 1)] = 0.5f * (x[grid.IX3(i, N, N + 1)] + x[grid.IX3(i, N + 1, N)]);
    }

    // Corners (average of three adjacent edge cells)
    x[grid.IX3(0, 0, 0)]             = (x[grid.IX3(1, 0, 0)] + x[grid.IX3(0, 1, 0)] + x[grid.IX3(0, 0, 1)]) / 3.0f;
    x[grid.IX3(0, N + 1, 0)]         = (x[grid.IX3(1, N + 1, 0)] + x[grid.IX3(0, N, 0)] + x[grid.IX3(0, N + 1, 1)]) / 3.0f;
    x[grid.IX3(N + 1, 0, 0)]         = (x[grid.IX3(N, 0, 0)] + x[grid.IX3(N + 1, 1, 0)] + x[grid.IX3(N + 1, 0, 1)]) / 3.0f;
    x[grid.IX3(N + 1, N + 1, 0)]     = (x[grid.IX3(N, N + 1, 0)] + x[grid.IX3(N + 1, N, 0)] + x[grid.IX3(N + 1, N + 1, 1)]) / 3.0f;
    x[grid.IX3(0, 0, N + 1)]         = (x[grid.IX3(1, 0, N + 1)] + x[grid.IX3(0, 1, N + 1)] + x[grid.IX3(0, 0, N)]) / 3.0f;
    x[grid.IX3(0, N + 1, N + 1)]     = (x[grid.IX3(1, N + 1, N + 1)] + x[grid.IX3(0, N, N + 1)] + x[grid.IX3(0, N + 1, N)]) / 3.0f;
    x[grid.IX3(N + 1, 0, N + 1)]     = (x[grid.IX3(N, 0, N + 1)] + x[grid.IX3(N + 1, 1, N + 1)] + x[grid.IX3(N + 1, 0, N)]) / 3.0f;
    x[grid.IX3(N + 1, N + 1, N + 1)] = (x[grid.IX3(N, N + 1, N + 1)] + x[grid.IX3(N + 1, N, N + 1)] + x[grid.IX3(N + 1, N + 1, N)]) / 3.0f;
}

} // namespace Effects
} // namespace Enjin
