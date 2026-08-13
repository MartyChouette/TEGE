#include "Enjin/Renderer/VoxelConeTracing.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/ECS/Components/Light.h"
#include "Enjin/Logging/Log.h"
#include <algorithm>
#include <cmath>

namespace Enjin {
namespace Renderer {

// Static empty voxel for out-of-bounds returns
VoxelData VoxelGrid::s_EmptyVoxel = {};

// ===========================================================================
// VoxelGrid Implementation
// ===========================================================================

void VoxelGrid::Initialize(i32 resolution, f32 worldExtent)
{
    // Clamp resolution to supported values
    if (resolution <= 64) m_Resolution = 64;
    else if (resolution <= 128) m_Resolution = 128;
    else m_Resolution = 256;

    m_WorldExtent = worldExtent;
    m_VoxelSize = worldExtent / static_cast<f32>(m_Resolution);

    // Grid is centered at origin
    f32 halfExtent = worldExtent * 0.5f;
    m_GridOrigin = Math::Vector3(-halfExtent, -halfExtent, -halfExtent);

    // Calculate mip levels: log2(resolution)
    m_MipLevels = 0;
    i32 dim = m_Resolution;
    while (dim >= 1) {
        m_MipLevels++;
        dim /= 2;
    }

    // Allocate mip chain
    m_MipChain.resize(static_cast<usize>(m_MipLevels));
    dim = m_Resolution;
    for (i32 mip = 0; mip < m_MipLevels; ++mip) {
        usize count = static_cast<usize>(dim) * dim * dim;
        m_MipChain[mip].resize(count);
        dim /= 2;
        if (dim < 1) dim = 1;
    }

    Clear();

    ENJIN_LOG_INFO(Renderer, "VoxelGrid initialized: %dx%dx%d, extent=%.1f, voxelSize=%.4f, %d mip levels",
                   m_Resolution, m_Resolution, m_Resolution,
                   m_WorldExtent, m_VoxelSize, m_MipLevels);
}

void VoxelGrid::Clear()
{
    for (auto& mip : m_MipChain) {
        for (auto& voxel : mip) {
            voxel = VoxelData{};
        }
    }
}

void VoxelGrid::Voxelize(ECS::World* world)
{
    if (!world || m_Resolution == 0) return;

    // Clear the base level before revoxelization
    for (auto& voxel : m_MipChain[0]) {
        voxel = VoxelData{};
    }

    // Get all entities with both mesh and transform (reuse a buffer — no per-frame heap alloc).
    static std::vector<ECS::Entity> meshEntities;
    world->GetEntitiesWithComponents<ECS::MeshComponent, ECS::TransformComponent>(meshEntities);

    for (ECS::Entity entity : meshEntities) {
        auto* mesh = world->GetComponent<ECS::MeshComponent>(entity);
        auto* transform = world->GetComponent<ECS::TransformComponent>(entity);
        if (!mesh || !transform || !transform->visible) continue;
        if (!mesh->IsValid()) continue;

        Math::Matrix4 modelMatrix = transform->ToMatrix();

        // Default color: white; use material base color if available
        Math::Vector3 baseColor(1.0f);
        auto* material = world->GetComponent<ECS::MaterialComponent>(entity);
        if (material) {
            baseColor = material->baseColor;
        }

        // Iterate triangles
        const auto& vertices = mesh->vertices;
        const auto& indices = mesh->indices;

        for (usize i = 0; i + 2 < indices.size(); i += 3) {
            // Transform vertices to world space
            Math::Vector4 v0w = modelMatrix * Math::Vector4(vertices[indices[i]].position, 1.0f);
            Math::Vector4 v1w = modelMatrix * Math::Vector4(vertices[indices[i + 1]].position, 1.0f);
            Math::Vector4 v2w = modelMatrix * Math::Vector4(vertices[indices[i + 2]].position, 1.0f);

            Math::Vector3 v0(v0w.x, v0w.y, v0w.z);
            Math::Vector3 v1(v1w.x, v1w.y, v1w.z);
            Math::Vector3 v2(v2w.x, v2w.y, v2w.z);

            // Average vertex color for the triangle, or use material base color
            Math::Vector3 triColor = baseColor;
            Math::Vector4 vc0 = vertices[indices[i]].color;
            Math::Vector4 vc1 = vertices[indices[i + 1]].color;
            Math::Vector4 vc2 = vertices[indices[i + 2]].color;
            // Blend vertex colors if non-white
            Math::Vector3 avgVertColor(
                (vc0.x + vc1.x + vc2.x) / 3.0f,
                (vc0.y + vc1.y + vc2.y) / 3.0f,
                (vc0.z + vc1.z + vc2.z) / 3.0f
            );
            triColor = Math::Vector3(
                triColor.x * avgVertColor.x,
                triColor.y * avgVertColor.y,
                triColor.z * avgVertColor.z
            );

            RasterizeTriangle(v0, v1, v2, triColor);
        }
    }
}

void VoxelGrid::InjectDirectLighting(const std::vector<VXGILight>& lights)
{
    if (m_Resolution == 0) return;

    i32 res = m_Resolution;
    auto& voxels = m_MipChain[0];

    for (i32 z = 0; z < res; ++z) {
        for (i32 y = 0; y < res; ++y) {
            for (i32 x = 0; x < res; ++x) {
                usize idx = static_cast<usize>(x + y * res + z * res * res);
                VoxelData& voxel = voxels[idx];

                // Skip empty voxels
                if (voxel.opacity < Math::EPSILON) continue;

                // World position of voxel center
                Math::Vector3 worldPos(
                    m_GridOrigin.x + (static_cast<f32>(x) + 0.5f) * m_VoxelSize,
                    m_GridOrigin.y + (static_cast<f32>(y) + 0.5f) * m_VoxelSize,
                    m_GridOrigin.z + (static_cast<f32>(z) + 0.5f) * m_VoxelSize
                );

                // Accumulate lighting from all sources
                Math::Vector3 litColor(0.0f);

                for (const auto& light : lights) {
                    Math::Vector3 lightDir;
                    f32 attenuation = 1.0f;

                    if (light.isDirectional) {
                        lightDir = -light.direction;
                        attenuation = 1.0f;
                    } else {
                        Math::Vector3 toLight = light.position - worldPos;
                        f32 dist = toLight.Length();
                        if (dist > light.range || dist < Math::EPSILON) continue;
                        lightDir = toLight / dist;
                        // Quadratic attenuation with range falloff
                        f32 rangeFactor = 1.0f - Math::Clamp(dist / light.range, 0.0f, 1.0f);
                        attenuation = rangeFactor * rangeFactor;
                    }

                    // Simple shadow test: march from voxel toward light,
                    // checking for occluding voxels (simplified for CPU perf)
                    bool shadowed = false;
                    f32 shadowDist = light.isDirectional ? m_WorldExtent : (light.position - worldPos).Length();
                    f32 step = m_VoxelSize * 2.0f;
                    Math::Vector3 shadowPos = worldPos + lightDir * m_VoxelSize; // Start slightly offset

                    for (f32 d = m_VoxelSize; d < shadowDist; d += step) {
                        i32 gx, gy, gz;
                        if (WorldToGrid(shadowPos, 0, gx, gy, gz)) {
                            usize sIdx = static_cast<usize>(gx + gy * res + gz * res * res);
                            if (voxels[sIdx].opacity > 0.5f) {
                                shadowed = true;
                                break;
                            }
                        }
                        shadowPos += lightDir * step;
                    }

                    if (!shadowed) {
                        // Lambertian lighting (using a rough normal approximation
                        // from the gradient of occupied voxels)
                        f32 nDotL = Math::Max(0.0f, 0.5f); // Simplified: assume average facing
                        litColor += Math::Vector3(
                            light.color.x * light.intensity * attenuation * nDotL,
                            light.color.y * light.intensity * attenuation * nDotL,
                            light.color.z * light.intensity * attenuation * nDotL
                        );
                    }
                }

                // Modulate voxel color by lighting (albedo * light)
                voxel.color = Math::Vector3(
                    voxel.color.x * (litColor.x + 0.1f), // Ambient floor of 0.1
                    voxel.color.y * (litColor.y + 0.1f),
                    voxel.color.z * (litColor.z + 0.1f)
                );
            }
        }
    }
}

void VoxelGrid::GenerateMipmaps()
{
    for (i32 mip = 1; mip < m_MipLevels; ++mip) {
        i32 srcRes = m_Resolution >> (mip - 1);
        i32 dstRes = m_Resolution >> mip;
        if (srcRes < 2) srcRes = 2;
        if (dstRes < 1) dstRes = 1;

        auto& srcData = m_MipChain[static_cast<usize>(mip - 1)];
        auto& dstData = m_MipChain[static_cast<usize>(mip)];

        for (i32 z = 0; z < dstRes; ++z) {
            for (i32 y = 0; y < dstRes; ++y) {
                for (i32 x = 0; x < dstRes; ++x) {
                    // Average 2x2x2 block from the source mip
                    Math::Vector3 colorSum(0.0f);
                    f32 opacitySum = 0.0f;
                    f32 emissionSum = 0.0f;
                    i32 count = 0;

                    for (i32 dz = 0; dz < 2; ++dz) {
                        for (i32 dy = 0; dy < 2; ++dy) {
                            for (i32 dx = 0; dx < 2; ++dx) {
                                i32 sx = x * 2 + dx;
                                i32 sy = y * 2 + dy;
                                i32 sz = z * 2 + dz;
                                if (sx < srcRes && sy < srcRes && sz < srcRes) {
                                    usize sIdx = static_cast<usize>(sx + sy * srcRes + sz * srcRes * srcRes);
                                    const VoxelData& src = srcData[sIdx];
                                    colorSum += src.color;
                                    opacitySum += src.opacity;
                                    emissionSum += src.emission;
                                    count++;
                                }
                            }
                        }
                    }

                    usize dIdx = static_cast<usize>(x + y * dstRes + z * dstRes * dstRes);
                    if (count > 0) {
                        f32 invCount = 1.0f / static_cast<f32>(count);
                        dstData[dIdx].color = colorSum * invCount;
                        dstData[dIdx].opacity = opacitySum * invCount;
                        dstData[dIdx].emission = emissionSum * invCount;
                    } else {
                        dstData[dIdx] = VoxelData{};
                    }
                }
            }
        }
    }
}

VoxelData VoxelGrid::SampleVoxel(const Math::Vector3& worldPos, i32 mipLevel) const
{
    mipLevel = Math::Clamp(static_cast<f32>(mipLevel), 0.0f, static_cast<f32>(m_MipLevels - 1));

    i32 gx, gy, gz;
    if (!WorldToGrid(worldPos, mipLevel, gx, gy, gz)) {
        return s_EmptyVoxel;
    }

    i32 res = m_Resolution >> mipLevel;
    if (res < 1) res = 1;
    usize idx = static_cast<usize>(gx + gy * res + gz * res * res);
    return m_MipChain[static_cast<usize>(mipLevel)][idx];
}

VoxelData VoxelGrid::SampleVoxelTrilinear(const Math::Vector3& worldPos, f32 mipLevel) const
{
    i32 mip0 = static_cast<i32>(Math::Floor(mipLevel));
    i32 mip1 = mip0 + 1;
    f32 mipFrac = mipLevel - static_cast<f32>(mip0);

    mip0 = Math::Clamp(static_cast<f32>(mip0), 0.0f, static_cast<f32>(m_MipLevels - 1));
    mip1 = Math::Clamp(static_cast<f32>(mip1), 0.0f, static_cast<f32>(m_MipLevels - 1));

    VoxelData v0 = SampleVoxel(worldPos, mip0);
    VoxelData v1 = SampleVoxel(worldPos, mip1);

    VoxelData result;
    result.color = Math::Lerp(v0.color, v1.color, mipFrac);
    result.opacity = Math::Lerp(v0.opacity, v1.opacity, mipFrac);
    result.emission = Math::Lerp(v0.emission, v1.emission, mipFrac);
    return result;
}

bool VoxelGrid::WorldToGrid(const Math::Vector3& worldPos, i32 mipLevel, i32& gx, i32& gy, i32& gz) const
{
    i32 res = m_Resolution >> mipLevel;
    if (res < 1) res = 1;

    f32 mipVoxelSize = m_WorldExtent / static_cast<f32>(res);

    gx = static_cast<i32>((worldPos.x - m_GridOrigin.x) / mipVoxelSize);
    gy = static_cast<i32>((worldPos.y - m_GridOrigin.y) / mipVoxelSize);
    gz = static_cast<i32>((worldPos.z - m_GridOrigin.z) / mipVoxelSize);

    if (gx < 0 || gx >= res || gy < 0 || gy >= res || gz < 0 || gz >= res) {
        return false;
    }
    return true;
}

VoxelData& VoxelGrid::GetVoxel(i32 x, i32 y, i32 z)
{
    if (x < 0 || x >= m_Resolution || y < 0 || y >= m_Resolution || z < 0 || z >= m_Resolution) {
        return s_EmptyVoxel;
    }
    usize idx = static_cast<usize>(x + y * m_Resolution + z * m_Resolution * m_Resolution);
    return m_MipChain[0][idx];
}

const VoxelData& VoxelGrid::GetVoxel(i32 x, i32 y, i32 z) const
{
    if (x < 0 || x >= m_Resolution || y < 0 || y >= m_Resolution || z < 0 || z >= m_Resolution) {
        return s_EmptyVoxel;
    }
    usize idx = static_cast<usize>(x + y * m_Resolution + z * m_Resolution * m_Resolution);
    return m_MipChain[0][idx];
}

VoxelData& VoxelGrid::GetMipVoxel(i32 mipLevel, i32 x, i32 y, i32 z)
{
    if (mipLevel < 0 || mipLevel >= m_MipLevels) return s_EmptyVoxel;

    i32 res = m_Resolution >> mipLevel;
    if (res < 1) res = 1;
    if (x < 0 || x >= res || y < 0 || y >= res || z < 0 || z >= res) {
        return s_EmptyVoxel;
    }
    usize idx = static_cast<usize>(x + y * res + z * res * res);
    return m_MipChain[static_cast<usize>(mipLevel)][idx];
}

const VoxelData& VoxelGrid::GetMipVoxel(i32 mipLevel, i32 x, i32 y, i32 z) const
{
    if (mipLevel < 0 || mipLevel >= m_MipLevels) return s_EmptyVoxel;

    i32 res = m_Resolution >> mipLevel;
    if (res < 1) res = 1;
    if (x < 0 || x >= res || y < 0 || y >= res || z < 0 || z >= res) {
        return s_EmptyVoxel;
    }
    usize idx = static_cast<usize>(x + y * res + z * res * res);
    return m_MipChain[static_cast<usize>(mipLevel)][idx];
}

void VoxelGrid::RasterizeTriangle(
    const Math::Vector3& v0,
    const Math::Vector3& v1,
    const Math::Vector3& v2,
    const Math::Vector3& color)
{
    if (m_Resolution == 0) return;

    // Compute triangle AABB in grid space
    Math::Vector3 triMin(
        Math::Min(v0.x, Math::Min(v1.x, v2.x)),
        Math::Min(v0.y, Math::Min(v1.y, v2.y)),
        Math::Min(v0.z, Math::Min(v1.z, v2.z))
    );
    Math::Vector3 triMax(
        Math::Max(v0.x, Math::Max(v1.x, v2.x)),
        Math::Max(v0.y, Math::Max(v1.y, v2.y)),
        Math::Max(v0.z, Math::Max(v1.z, v2.z))
    );

    // Convert to grid coordinates
    i32 minX = static_cast<i32>((triMin.x - m_GridOrigin.x) / m_VoxelSize);
    i32 minY = static_cast<i32>((triMin.y - m_GridOrigin.y) / m_VoxelSize);
    i32 minZ = static_cast<i32>((triMin.z - m_GridOrigin.z) / m_VoxelSize);
    i32 maxX = static_cast<i32>((triMax.x - m_GridOrigin.x) / m_VoxelSize);
    i32 maxY = static_cast<i32>((triMax.y - m_GridOrigin.y) / m_VoxelSize);
    i32 maxZ = static_cast<i32>((triMax.z - m_GridOrigin.z) / m_VoxelSize);

    // Clamp to grid bounds
    minX = std::max(minX, 0);
    minY = std::max(minY, 0);
    minZ = std::max(minZ, 0);
    maxX = std::min(maxX, m_Resolution - 1);
    maxY = std::min(maxY, m_Resolution - 1);
    maxZ = std::min(maxZ, m_Resolution - 1);

    f32 halfVoxel = m_VoxelSize * 0.5f;
    Math::Vector3 halfSize(halfVoxel);

    // Conservative rasterization: test triangle-AABB overlap for each candidate voxel
    for (i32 z = minZ; z <= maxZ; ++z) {
        for (i32 y = minY; y <= maxY; ++y) {
            for (i32 x = minX; x <= maxX; ++x) {
                Math::Vector3 voxelCenter(
                    m_GridOrigin.x + (static_cast<f32>(x) + 0.5f) * m_VoxelSize,
                    m_GridOrigin.y + (static_cast<f32>(y) + 0.5f) * m_VoxelSize,
                    m_GridOrigin.z + (static_cast<f32>(z) + 0.5f) * m_VoxelSize
                );

                if (TriangleAABBOverlap(v0, v1, v2, voxelCenter, halfSize)) {
                    usize idx = static_cast<usize>(x + y * m_Resolution + z * m_Resolution * m_Resolution);
                    VoxelData& voxel = m_MipChain[0][idx];
                    // Blend color additively for overlapping triangles, average later
                    if (voxel.opacity < Math::EPSILON) {
                        voxel.color = color;
                        voxel.opacity = 1.0f;
                    } else {
                        // Average with existing color
                        voxel.color = (voxel.color + color) * 0.5f;
                    }
                }
            }
        }
    }
}

bool VoxelGrid::TriangleAABBOverlap(
    const Math::Vector3& v0,
    const Math::Vector3& v1,
    const Math::Vector3& v2,
    const Math::Vector3& boxCenter,
    const Math::Vector3& boxHalfSize)
{
    // Simplified separating axis test (Tomas Akenine-Moller)
    // Test AABB axes, triangle normal, and cross products of edges x AABB axes

    // Translate triangle to AABB-centered coordinates
    Math::Vector3 t0 = v0 - boxCenter;
    Math::Vector3 t1 = v1 - boxCenter;
    Math::Vector3 t2 = v2 - boxCenter;

    // Test AABB axes (X, Y, Z)
    // X axis
    f32 minVal = Math::Min(t0.x, Math::Min(t1.x, t2.x));
    f32 maxVal = Math::Max(t0.x, Math::Max(t1.x, t2.x));
    if (minVal > boxHalfSize.x || maxVal < -boxHalfSize.x) return false;

    // Y axis
    minVal = Math::Min(t0.y, Math::Min(t1.y, t2.y));
    maxVal = Math::Max(t0.y, Math::Max(t1.y, t2.y));
    if (minVal > boxHalfSize.y || maxVal < -boxHalfSize.y) return false;

    // Z axis
    minVal = Math::Min(t0.z, Math::Min(t1.z, t2.z));
    maxVal = Math::Max(t0.z, Math::Max(t1.z, t2.z));
    if (minVal > boxHalfSize.z || maxVal < -boxHalfSize.z) return false;

    // Test triangle normal
    Math::Vector3 e0 = t1 - t0;
    Math::Vector3 e1 = t2 - t1;
    Math::Vector3 normal = e0.Cross(e1);

    f32 d = normal.Dot(t0);
    f32 r = boxHalfSize.x * Math::Abs(normal.x) +
            boxHalfSize.y * Math::Abs(normal.y) +
            boxHalfSize.z * Math::Abs(normal.z);
    if (Math::Abs(d) > r) return false;

    // Test 9 cross-product axes (3 edges x 3 AABB axes)
    // For simplicity and CPU performance, use a conservative approach:
    // The AABB and normal tests catch most separations. The cross-product
    // tests are omitted here for a slightly conservative but much faster result.
    // This means some voxels may be filled that shouldn't be, but for GI
    // purposes this over-estimation is acceptable.

    return true;
}

// ===========================================================================
// ConeTracer Implementation
// ===========================================================================

ConeTraceResult ConeTracer::TraceCone(
    const Math::Vector3& origin,
    const Math::Vector3& direction,
    f32 aperture,
    f32 maxDistance) const
{
    ConeTraceResult result;
    if (!m_Grid || m_Grid->GetResolution() == 0) return result;

    f32 voxelSize = m_Grid->GetVoxelSize();
    f32 tanAperture = std::tan(aperture);

    // Start with a small offset to avoid self-intersection
    f32 dist = voxelSize;

    // Front-to-back compositing
    Math::Vector3 accColor(0.0f);
    f32 accOpacity = 0.0f;

    while (dist < maxDistance && accOpacity < 0.99f) {
        // Cone radius at current distance
        f32 coneRadius = dist * tanAperture;

        // Determine mip level: we want to sample at a resolution where
        // one voxel roughly matches the cone's cross-section diameter
        f32 mipLevelF = Math::Log2(Math::Max(1.0f, coneRadius / voxelSize));
        mipLevelF = Math::Clamp(mipLevelF, 0.0f, static_cast<f32>(m_Grid->GetMipLevels() - 1));

        // Sample position
        Math::Vector3 samplePos = origin + direction * dist;

        // Sample voxel with trilinear filtering across mip levels
        VoxelData sample = m_Grid->SampleVoxelTrilinear(samplePos, mipLevelF);

        // Front-to-back compositing (alpha blending)
        if (sample.opacity > Math::EPSILON) {
            f32 a = sample.opacity * (1.0f - accOpacity);
            accColor += (sample.color + Math::Vector3(sample.emission)) * a;
            accOpacity += a;
        }

        // Step forward: step size scales with cone radius for efficiency
        // Smaller steps near the origin (fine detail), larger steps further out
        f32 stepSize = Math::Max(voxelSize, coneRadius * 0.5f);
        dist += stepSize;
    }

    result.color = accColor;
    result.occlusion = accOpacity;
    return result;
}

Math::Vector3 ConeTracer::ComputeDiffuseGI(
    const Math::Vector3& position,
    const Math::Vector3& normal) const
{
    if (!m_Grid) return Math::Vector3(0.0f);

    // Build tangent frame from normal
    Math::Vector3 tangent, bitangent;
    BuildOrthonormalBasis(normal, tangent, bitangent);

    i32 numCones = Math::Clamp(static_cast<f32>(m_Config.diffuseCones), 1.0f, 16.0f);
    f32 aperture = m_Config.coneSpread;

    Math::Vector3 irradiance(0.0f);

    // Distribute cones in a hemisphere around the normal.
    // Use a fixed pattern for consistency:
    // 1 cone straight up + (numCones-1) cones at an angle around the normal
    if (numCones >= 1) {
        // Central cone along normal
        ConeTraceResult r = TraceCone(position, normal, aperture, m_Config.traceDistance);
        irradiance += r.color;
    }

    if (numCones > 1) {
        f32 sideAngle = Math::PI / 3.0f; // 60 degrees from normal
        f32 sinSide = Math::Sin(sideAngle);
        f32 cosSide = Math::Cos(sideAngle);

        i32 sideCones = numCones - 1;
        f32 angleStep = Math::PI_2 / static_cast<f32>(sideCones);

        for (i32 i = 0; i < sideCones; ++i) {
            f32 phi = static_cast<f32>(i) * angleStep;
            Math::Vector3 dir =
                normal * cosSide +
                tangent * (sinSide * Math::Cos(phi)) +
                bitangent * (sinSide * Math::Sin(phi));
            dir.Normalize();

            ConeTraceResult r = TraceCone(position, dir, aperture, m_Config.traceDistance);
            irradiance += r.color;
        }
    }

    // Average and scale
    f32 invCones = 1.0f / static_cast<f32>(numCones);
    irradiance *= invCones * m_Config.indirectMultiplier;

    return irradiance;
}

Math::Vector3 ConeTracer::ComputeSpecularGI(
    const Math::Vector3& position,
    const Math::Vector3& normal,
    const Math::Vector3& viewDir,
    f32 roughness) const
{
    if (!m_Grid) return Math::Vector3(0.0f);

    // Compute reflection direction
    f32 nDotV = normal.Dot(viewDir);
    Math::Vector3 reflection = viewDir - normal * (2.0f * nDotV);
    reflection.Normalize();

    // Aperture based on roughness: smooth = narrow cone, rough = wide cone
    f32 aperture = Math::Clamp(roughness * roughness, 0.01f, 0.5f);

    ConeTraceResult r = TraceCone(position, reflection, aperture, m_Config.traceDistance);

    return r.color * m_Config.indirectMultiplier;
}

f32 ConeTracer::ComputeAmbientOcclusion(
    const Math::Vector3& position,
    const Math::Vector3& normal) const
{
    if (!m_Grid) return 0.0f;

    Math::Vector3 tangent, bitangent;
    BuildOrthonormalBasis(normal, tangent, bitangent);

    // Trace short cones in hemisphere to measure occlusion
    f32 aoDistance = m_Grid->GetVoxelSize() * 10.0f; // Short range
    f32 aperture = 0.5f; // Wide cones for AO

    f32 totalOcclusion = 0.0f;
    i32 numCones = 5; // Normal + 4 side cones

    // Central cone
    ConeTraceResult r = TraceCone(position, normal, aperture, aoDistance);
    totalOcclusion += r.occlusion;

    // Side cones at 60 degrees
    f32 sideAngle = Math::PI / 3.0f;
    f32 sinSide = Math::Sin(sideAngle);
    f32 cosSide = Math::Cos(sideAngle);

    for (i32 i = 0; i < 4; ++i) {
        f32 phi = static_cast<f32>(i) * Math::PI_2 / 4.0f;
        Math::Vector3 dir =
            normal * cosSide +
            tangent * (sinSide * Math::Cos(phi)) +
            bitangent * (sinSide * Math::Sin(phi));
        dir.Normalize();

        r = TraceCone(position, dir, aperture, aoDistance);
        totalOcclusion += r.occlusion;
    }

    return Math::Clamp(totalOcclusion / static_cast<f32>(numCones), 0.0f, 1.0f);
}

Math::Vector3 ConeTracer::ComputeGodRays(
    const Math::Vector3& lightPos,
    const Math::Vector3& viewPos,
    i32 numSteps) const
{
    if (!m_Grid) return Math::Vector3(0.0f);

    numSteps = Math::Clamp(static_cast<f32>(numSteps), 1.0f, 256.0f);

    Math::Vector3 rayDir = (lightPos - viewPos);
    f32 totalDist = rayDir.Length();
    if (totalDist < Math::EPSILON) return Math::Vector3(0.0f);
    rayDir = rayDir / totalDist;

    f32 stepSize = totalDist / static_cast<f32>(numSteps);

    Math::Vector3 scattered(0.0f);
    f32 transmittance = 1.0f;

    // Volumetric ray march from view toward light
    for (i32 i = 0; i < numSteps; ++i) {
        f32 t = (static_cast<f32>(i) + 0.5f) * stepSize;
        Math::Vector3 samplePos = viewPos + rayDir * t;

        VoxelData voxel = m_Grid->SampleVoxel(samplePos, 0);

        if (voxel.opacity > Math::EPSILON) {
            // Accumulate scattered light (Beer-Lambert law)
            f32 absorption = voxel.opacity * stepSize;
            transmittance *= Math::Exp(-absorption);

            // In-scattering: light scattered toward viewer
            scattered += voxel.color * (voxel.opacity * transmittance * stepSize);
        }

        // Early termination if fully absorbed
        if (transmittance < 0.01f) break;
    }

    return scattered * m_Config.indirectMultiplier;
}

void ConeTracer::BuildOrthonormalBasis(
    const Math::Vector3& normal,
    Math::Vector3& tangent,
    Math::Vector3& bitangent)
{
    // Frisvad's method for building an orthonormal basis from a single vector
    if (normal.y < -0.9999f) {
        tangent = Math::Vector3(0, 0, -1);
        bitangent = Math::Vector3(-1, 0, 0);
        return;
    }

    f32 a = 1.0f / (1.0f + normal.y);
    f32 b = -normal.x * normal.z * a;

    tangent = Math::Vector3(1.0f - normal.x * normal.x * a, -normal.x, b);
    bitangent = Math::Vector3(b, -normal.z, 1.0f - normal.z * normal.z * a);

    tangent.Normalize();
    bitangent.Normalize();
}

} // namespace Renderer
} // namespace Enjin
