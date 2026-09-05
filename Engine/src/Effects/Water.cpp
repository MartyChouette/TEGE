#include "Enjin/Effects/Water.h"
#include "Enjin/ECS/Components/Water3D.h"
#include "Enjin/Math/Math.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/Material.h"
#include <algorithm>

namespace Enjin {
namespace Effects {

void Water3D::Initialize(const Water3DSettings& settings) {
    m_Settings = settings;
    m_Effective = settings;
    m_WaveTime = 0.0f;
    m_UVOffset = Math::Vector2(0, 0);
}

void Water3D::SetWind(const Math::Vector3& direction, f32 strength) {
    m_WindDirection = direction;
    m_WindStrength = strength < 0.0f ? 0.0f : strength;
}

void Water3D::RecomputeEffectiveSettings() {
    m_Effective = m_Settings;

    const f32 influence = (m_Settings.windInfluence < 0.0f) ? 0.0f
                        : (m_Settings.windInfluence > 1.0f ? 1.0f : m_Settings.windInfluence);
    if (influence <= 0.0f) return;   // authored waves, untouched

    // Heading. The wind is a world vector; waveDirection is the 2D heading the
    // wave functions use, so only the horizontal part matters. A vertical or
    // zero wind leaves the authored heading alone rather than snapping the sea
    // to an arbitrary direction.
    const f32 hx = m_WindDirection.x, hz = m_WindDirection.z;
    const f32 hlen = std::sqrt(hx * hx + hz * hz);
    if (hlen > 1e-4f) {
        const Math::Vector2 windDir(hx / hlen, hz / hlen);
        m_Effective.waveDirection.x =
            m_Effective.waveDirection.x + (windDir.x - m_Effective.waveDirection.x) * influence;
        m_Effective.waveDirection.y =
            m_Effective.waveDirection.y + (windDir.y - m_Effective.waveDirection.y) * influence;
    }

    // Size and pace. Calm air flattens the sea toward still; a gale raises it.
    // Blended by influence so a partly wind-driven water still keeps some of
    // the swell the author asked for.
    const f32 heightFactor = 1.0f + (m_Settings.windWaveHeightScale - 1.0f) * m_WindStrength;
    const f32 speedFactor  = 1.0f + (m_Settings.windWaveSpeedScale  - 1.0f) * m_WindStrength;
    m_Effective.waveHeight = m_Effective.waveHeight * (1.0f + (heightFactor - 1.0f) * influence);
    m_Effective.waveSpeed  = m_Settings.waveSpeed  * (1.0f + (speedFactor  - 1.0f) * influence);
}

void Water3D::Update(f32 deltaTime) {
    // Fold wind in FIRST: the wave clock advances at the effective speed, so a
    // gust makes the surface move faster rather than just taller.
    RecomputeEffectiveSettings();

    m_WaveTime += deltaTime * m_Effective.waveSpeed;

    // Update UV scrolling
    m_UVOffset.x += m_Settings.uvScrollSpeed.x * deltaTime;
    m_UVOffset.y += m_Settings.uvScrollSpeed.y * deltaTime;

    // Wrap UV offset
    if (m_UVOffset.x > 1.0f) m_UVOffset.x -= 1.0f;
    if (m_UVOffset.y > 1.0f) m_UVOffset.y -= 1.0f;
}

f32 Water3D::SampleWaveHeight(const Water3DSettings& settings, f32 waveTime, f32 x, f32 z) {
    if (settings.gerstnerWaves) {
        // Vertical component of the trochoidal surface. Ignores the horizontal
        // shift (inverting it needs iteration) — close enough for buoyancy.
        Water3D tmp;
        tmp.m_Settings = settings;
        tmp.m_Effective = settings;
        tmp.m_WaveTime = waveTime;
        return tmp.GetGerstnerOffset(x, z).y;
    }

    // Simple sine wave combination (very PS1/N64)
    const f32 wave1 = Math::Sin((x * settings.waveFrequency + waveTime) * settings.waveDirection.x);
    const f32 wave2 = Math::Sin((z * settings.waveFrequency * 0.7f + waveTime * 1.3f) * settings.waveDirection.y);
    const f32 wave3 = Math::Sin((x + z) * settings.waveFrequency * 0.5f + waveTime * 0.8f) * 0.5f;

    return (wave1 + wave2 + wave3) * settings.waveHeight / 2.5f;
}

f32 Water3D::GetWaveHeight(f32 x, f32 z) const {
    // Through the shared sampler, on the EFFECTIVE settings, so the height a
    // script or the buoyancy asks for is the height the mesh was built at.
    return SampleWaveHeight(m_Effective, m_WaveTime, x, z);
}

Math::Vector3 Water3D::GetGerstnerOffset(f32 x, f32 z) const {
    // Three Gerstner waves derived from the sine-wave parameters: the primary
    // wave follows waveDirection, plus two smaller waves at rotated headings
    // and higher frequencies so the surface doesn't read as parallel rollers.
    // Each wave moves a vertex ALONG its travel direction by Q*A*cos(phase)
    // (bunching vertices at crests = sharp peaks, spreading them in troughs =
    // flat valleys) and lifts it by A*sin(phase).
    struct GerstnerWave { f32 dirX, dirZ, amplitude, frequency, phaseSpeed; };
    Math::Vector2 d = m_Effective.waveDirection;
    f32 dLen = Math::Sqrt(d.x * d.x + d.y * d.y);
    if (dLen < 0.0001f) { d = Math::Vector2(1.0f, 0.0f); dLen = 1.0f; }
    f32 dx = d.x / dLen, dz = d.y / dLen;

    const f32 A = m_Effective.waveHeight;
    const f32 w = Math::Max(m_Effective.waveFrequency, 0.0001f);
    const GerstnerWave waves[3] = {
        { dx, dz, A * 0.60f, w, 1.0f },
        // ~60 degrees off the primary heading, tighter and faster
        { dx * 0.5f - dz * 0.866f, dx * 0.866f + dz * 0.5f, A * 0.30f, w * 1.9f, 1.3f },
        // ~-45 degrees, smallest ripple layer
        { dx * 0.707f + dz * 0.707f, -dx * 0.707f + dz * 0.707f, A * 0.15f, w * 3.1f, 0.8f },
    };

    // Per-wave steepness Q_i = steepness / (w_i * A_i * numWaves) is the
    // classic normalization that keeps the summed horizontal displacement
    // below the self-intersection limit (loops forming at crests) at
    // steepness <= 1 regardless of amplitude/frequency choices.
    f32 steep = Math::Clamp(m_Effective.waveSteepness, 0.0f, 1.0f);

    Math::Vector3 offset(0.0f, 0.0f, 0.0f);
    for (const auto& gw : waves) {
        f32 phase = gw.frequency * (gw.dirX * x + gw.dirZ * z) + m_WaveTime * gw.phaseSpeed;
        f32 q = (gw.amplitude > 0.0001f)
            ? steep / (gw.frequency * gw.amplitude * 3.0f)
            : 0.0f;
        f32 qa = q * gw.amplitude;
        f32 c = Math::Cos(phase);
        offset.x += gw.dirX * qa * c;
        offset.z += gw.dirZ * qa * c;
        offset.y += gw.amplitude * Math::Sin(phase);
    }
    return offset;
}

Math::Matrix4 Water3D::GetReflectionMatrix() const {
    // Reflection matrix for planar reflection at water surface
    // Reflects across the XZ plane at water Y position
    Math::Matrix4 reflect;
    f32 waterY = m_Settings.position.y;

    // Scale Y by -1, translate to water level
    reflect.m[0] = 1.0f;  reflect.m[1] = 0.0f;  reflect.m[2] = 0.0f;  reflect.m[3] = 0.0f;
    reflect.m[4] = 0.0f;  reflect.m[5] = -1.0f; reflect.m[6] = 0.0f;  reflect.m[7] = 0.0f;
    reflect.m[8] = 0.0f;  reflect.m[9] = 0.0f;  reflect.m[10] = 1.0f; reflect.m[11] = 0.0f;
    reflect.m[12] = 0.0f; reflect.m[13] = 2.0f * waterY; reflect.m[14] = 0.0f; reflect.m[15] = 1.0f;

    return reflect;
}

void Water3D::GenerateMesh(std::vector<Math::Vector3>& positions,
                           std::vector<Math::Vector2>& uvs,
                           std::vector<u32>& indices) const {
    positions.clear();
    uvs.clear();
    indices.clear();

    // Guard against zero/negative dimensions (prevents division by zero)
    f32 safeWidth = Math::Max(m_Settings.width, 0.01f);
    f32 safeDepth = Math::Max(m_Settings.depth, 0.01f);
    f32 safeTile  = Math::Max(m_Settings.tileSize, 0.01f);

    f32 halfWidth = safeWidth * 0.5f;
    f32 halfDepth = safeDepth * 0.5f;

    u32 xSegments = static_cast<u32>(safeWidth / safeTile);
    u32 zSegments = static_cast<u32>(safeDepth / safeTile);

    xSegments = Math::Max(xSegments, 1u);
    zSegments = Math::Max(zSegments, 1u);

    // Generate vertices
    for (u32 z = 0; z <= zSegments; ++z) {
        for (u32 x = 0; x <= xSegments; ++x) {
            f32 xPos = m_Settings.position.x - halfWidth + (static_cast<f32>(x) / xSegments) * safeWidth;
            f32 zPos = m_Settings.position.z - halfDepth + (static_cast<f32>(z) / zSegments) * safeDepth;
            f32 yPos = m_Settings.position.y;

            // Add wave displacement if using vertex waves
            if (m_Settings.style == WaterStyle::VertexWave ||
                m_Settings.style == WaterStyle::Reflective ||
                m_Settings.style == WaterStyle::Refractive) {
                if (m_Effective.gerstnerWaves) {
                    // Trochoidal: crests pull vertices horizontally too
                    Math::Vector3 off = GetGerstnerOffset(xPos, zPos);
                    positions.push_back(Math::Vector3(xPos + off.x, yPos + off.y, zPos + off.z));
                    f32 u = static_cast<f32>(x) / xSegments;
                    f32 v = static_cast<f32>(z) / zSegments;
                    uvs.push_back(Math::Vector2(u + m_UVOffset.x, v + m_UVOffset.y));
                    continue;
                }
                yPos += GetWaveHeight(xPos, zPos);
            }

            positions.push_back(Math::Vector3(xPos, yPos, zPos));

            f32 u = static_cast<f32>(x) / xSegments;
            f32 v = static_cast<f32>(z) / zSegments;
            uvs.push_back(Math::Vector2(u + m_UVOffset.x, v + m_UVOffset.y));
        }
    }

    // Generate indices
    for (u32 z = 0; z < zSegments; ++z) {
        for (u32 x = 0; x < xSegments; ++x) {
            u32 topLeft = z * (xSegments + 1) + x;
            u32 topRight = topLeft + 1;
            u32 bottomLeft = (z + 1) * (xSegments + 1) + x;
            u32 bottomRight = bottomLeft + 1;

            // Two triangles per quad
            indices.push_back(topLeft);
            indices.push_back(bottomLeft);
            indices.push_back(topRight);

            indices.push_back(topRight);
            indices.push_back(bottomLeft);
            indices.push_back(bottomRight);
        }
    }
}

void Water3D::BuildEntityMesh(ECS::World* world, ECS::Entity entity) const {
    if (!world || !world->IsValid(entity)) return;

    // Generate the mesh data from the CPU simulation
    std::vector<Math::Vector3> positions;
    std::vector<Math::Vector2> uvCoords;
    std::vector<u32> meshIndices;
    GenerateMesh(positions, uvCoords, meshIndices);

    // Build MeshComponent with full vertex attributes
    ECS::MeshComponent mesh;
    mesh.vertices.reserve(positions.size());

    f32 halfWidth = m_Settings.width * 0.5f;
    f32 halfDepth = m_Settings.depth * 0.5f;

    for (usize i = 0; i < positions.size(); ++i) {
        ECS::MeshComponent::Vertex v;
        v.position = positions[i];

        // Compute vertex normal from wave height gradient (finite differences)
        f32 eps = m_Settings.tileSize * 0.1f;
        f32 hL = GetWaveHeight(positions[i].x - eps, positions[i].z);
        f32 hR = GetWaveHeight(positions[i].x + eps, positions[i].z);
        f32 hD = GetWaveHeight(positions[i].x, positions[i].z - eps);
        f32 hU = GetWaveHeight(positions[i].x, positions[i].z + eps);
        Math::Vector3 normal(hL - hR, 2.0f * eps, hD - hU);
        v.normal = normal.Normalized();

        v.uv = uvCoords[i];

        // Edge distance for shore foam (same pattern as WaterVolumeComponent mesh)
        f32 safeW = Math::Max(m_Settings.width, 0.01f);
        f32 safeD = Math::Max(m_Settings.depth, 0.01f);
        f32 relX = (positions[i].x - (m_Settings.position.x - halfWidth)) / safeW;
        f32 relZ = (positions[i].z - (m_Settings.position.z - halfDepth)) / safeD;
        f32 distL = relX, distR = 1.0f - relX;
        f32 distT = relZ, distB = 1.0f - relZ;
        f32 minEdgeDist = Math::Min(Math::Min(distL, distR), Math::Min(distT, distB));
        f32 edgeDist = Math::Min(minEdgeDist * 2.0f, 1.0f);

        // Vertex colour contract for water surfaces, from triangle.frag: the
        // G channel carries EDGE DISTANCE (0 = edge, 1 = centre), RGB is not used
        // as colour (the shader skips the vertex tint for FLAG_WATER_SURFACE),
        // and ALPHA multiplies straight into opacity.
        //
        // This wrote shallowColor into RGB and edgeDist into ALPHA, which got
        // both halves wrong: the shore/foam path read shallowColor.g as its edge
        // distance - a constant, so shore never varied - and the real alpha
        // channel faded the surface out toward its own borders.
        //
        // The G term also folds in crest height. On an ocean the plane's border
        // is kilometres away and edge foam is useless, but a crest IS shallow
        // water as far as this shader is concerned, so wave tops pick up the
        // lighter tint and the foam and the troughs stay dark. That is the whole
        // depth read on a surface with no depth buffer to sample.
        f32 shallowness = edgeDist;
        {
            f32 amp = Math::Max(m_Effective.waveHeight, 0.0001f);
            f32 rel = (positions[i].y - m_Settings.position.y) / amp;   // -1 trough .. +1 crest
            f32 crest = Math::Clamp(rel * 0.5f + 0.5f, 0.0f, 1.0f);
            shallowness = Math::Min(shallowness, 1.0f - crest);
        }
        v.color = Math::Vector4(1.0f, shallowness, 1.0f, 1.0f);

        // Tangent along X direction
        v.tangent = Math::Vector4(1.0f, 0.0f, 0.0f, 1.0f);

        mesh.vertices.push_back(v);
    }

    mesh.indices = meshIndices;

    // Add or replace MeshComponent
    if (world->HasComponent<ECS::MeshComponent>(entity)) {
        *world->GetComponent<ECS::MeshComponent>(entity) = std::move(mesh);
    } else {
        world->AddComponent<ECS::MeshComponent>(entity, std::move(mesh));
    }

    // Create water material if not already present
    if (!world->HasComponent<ECS::MaterialComponent>(entity)) {
        ECS::MaterialComponent material;
        material.baseColor = m_Settings.shallowColor;
        material.opacity = m_Settings.opacity;
        // Matte, non-metallic: a mirror-smooth metallic surface throws a blown-out
        // specular/IBL sheen that reads as white once the water is translucent. Keep
        // the fresnel reflection (handled in the water shader) but not the hard glare.
        material.metallic = 0.0f;
        material.roughness = 0.45f;
        material.doubleSided = true;
        material.castShadows = false;
        // Translucent: blend so the Opacity setting shows the bottom through the
        // surface. Water still writes depth (the renderer keeps water surfaces on the
        // depth-writing pipeline even though they blend), so its own overlapping wave
        // fragments don't stack up and wash the plane out — that was why water was
        // pinned to Opaque before.
        material.alphaMode = ECS::MaterialComponent::AlphaMode::Blend;
        world->AddComponent<ECS::MaterialComponent>(entity, material);
    }
}

void Water3D::UpdateEntityMesh(ECS::World* world, ECS::Entity entity) const {
    if (!world) return;

    // Publish what this surface is actually doing, for physics to sample.
    // Buoyancy cannot call into this system, so the settings the mesh is being
    // built from -- wind already folded in -- and the wave clock go onto the
    // component. Written before the early-out below so a water entity that has
    // no mesh yet still floats things correctly.
    if (auto* w3 = world->GetComponent<ECS::Water3DComponent>(entity)) {
        w3->runtimeSettings = m_Effective;
        w3->runtimeWaveTime = m_WaveTime;
        w3->runtimeValid = true;
    }

    auto* mesh = world->GetComponent<ECS::MeshComponent>(entity);
    if (!mesh || mesh->vertices.empty()) return;

    // Recalculate the vertex positions and UVs from the current wave simulation
    std::vector<Math::Vector3> positions;
    std::vector<Math::Vector2> uvCoords;
    std::vector<u32> meshIndices;
    GenerateMesh(positions, uvCoords, meshIndices);

    // Update only positions, normals, and UVs (indices don't change)
    if (positions.size() != mesh->vertices.size()) return;

    f32 eps = m_Settings.tileSize * 0.1f;
    bool gerstner = m_Effective.gerstnerWaves &&
                    (m_Settings.style == WaterStyle::VertexWave ||
                     m_Settings.style == WaterStyle::Reflective ||
                     m_Settings.style == WaterStyle::Refractive);
    for (usize i = 0; i < positions.size(); ++i) {
        mesh->vertices[i].position = positions[i];
        mesh->vertices[i].uv = uvCoords[i];

        if (gerstner) {
            // Trochoidal surface: the horizontal displacement matters, so build
            // the normal from displaced-surface tangents instead of a height
            // gradient (which would soften the sharp crests this style is for).
            f32 px = positions[i].x, pz = positions[i].z;
            Math::Vector3 oL = GetGerstnerOffset(px - eps, pz);
            Math::Vector3 oR = GetGerstnerOffset(px + eps, pz);
            Math::Vector3 oD = GetGerstnerOffset(px, pz - eps);
            Math::Vector3 oU = GetGerstnerOffset(px, pz + eps);
            Math::Vector3 tx(2.0f * eps + oR.x - oL.x, oR.y - oL.y, oR.z - oL.z);
            Math::Vector3 tz(oU.x - oD.x, oU.y - oD.y, 2.0f * eps + oU.z - oD.z);
            // cross(tz, tx) points +Y for a flat surface
            mesh->vertices[i].normal = tz.Cross(tx).Normalized();
            continue;
        }

        // Recompute normal from wave gradient
        f32 hL = GetWaveHeight(positions[i].x - eps, positions[i].z);
        f32 hR = GetWaveHeight(positions[i].x + eps, positions[i].z);
        f32 hD = GetWaveHeight(positions[i].x, positions[i].z - eps);
        f32 hU = GetWaveHeight(positions[i].x, positions[i].z + eps);
        Math::Vector3 normal(hL - hR, 2.0f * eps, hD - hU);
        mesh->vertices[i].normal = normal.Normalized();
    }

    // Mark AABB as dirty so frustum culling recalculates
    mesh->aabbDirty = true;
}

// 2D Water Implementation
void Water2D::Initialize(const Water2DSettings& settings) {
    m_Settings = settings;
    m_Time = 0.0f;
}

void Water2D::Update(f32 deltaTime) {
    m_Time += deltaTime * m_Settings.waveSpeed;
}

f32 Water2D::GetWaveOffset(f32 x) const {
    if (!m_Settings.enableWaveTop) return 0.0f;

    // Simple sine wave for wavy top edge
    return Math::Sin(x * m_Settings.waveFrequency + m_Time) * m_Settings.waveAmplitude;
}

f32 Water2D::GetReflectionOffset(f32 x, f32 y) const {
    if (!m_Settings.enableReflection) return 0.0f;

    // Wobble effect for reflections
    f32 depth = y - m_Settings.position.y;
    return Math::Sin(x * 0.1f + m_Time * 2.0f + depth * 0.05f) * m_Settings.reflectionDistortion;
}

bool Water2D::IsUnderwater(const Math::Vector2& point) const {
    f32 waterTop = m_Settings.position.y + GetWaveOffset(point.x);
    return point.y > waterTop &&
           point.x >= m_Settings.position.x &&
           point.x <= m_Settings.position.x + m_Settings.size.x &&
           point.y <= m_Settings.position.y + m_Settings.size.y;
}

// Water Interaction
void WaterInteraction::SpawnSplash(const SplashSettings& settings) {
    // A splash creates a ripple at the impact point
    // The visual splash particles would be handled by a particle system if available,
    // but the water surface effect is represented as ripples
    RippleSettings ripple;
    ripple.position = settings.position;
    ripple.amplitude = settings.size * 0.5f;
    ripple.maxRadius = settings.size * 3.0f;
    ripple.duration = settings.duration;
    SpawnRipple(ripple);

    // For larger splashes, add secondary ripples
    if (settings.particleCount > 5) {
        RippleSettings secondary;
        secondary.position = settings.position;
        secondary.amplitude = settings.size * 0.25f;
        secondary.maxRadius = settings.size * 5.0f;
        secondary.duration = settings.duration * 1.5f;
        SpawnRipple(secondary);
    }
}

void WaterInteraction::SpawnRipple(const RippleSettings& settings) {
    ActiveRipple ripple;
    ripple.position = settings.position;
    ripple.currentRadius = 0.0f;
    ripple.amplitude = settings.amplitude;
    ripple.progress = 0.0f;

    m_Ripples.push_back(ripple);
}

void WaterInteraction::Update(f32 deltaTime) {
    // Update all ripples in a single pass
    for (auto& r : m_Ripples) {
        r.progress += deltaTime / 2.0f;  // 2 second duration
        r.currentRadius = r.progress * 5.0f;  // Expand radius
        r.amplitude *= (1.0f - deltaTime);  // Fade out
    }
    // Remove expired ripples in O(N) instead of O(N^2) iterator-erase
    m_Ripples.erase(
        std::remove_if(m_Ripples.begin(), m_Ripples.end(),
            [](const ActiveRipple& r) { return r.progress >= 1.0f; }),
        m_Ripples.end());
}

} // namespace Effects
} // namespace Enjin
