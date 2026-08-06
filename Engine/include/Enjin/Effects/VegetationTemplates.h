#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <vector>

namespace Enjin {
namespace Effects {

// Single source of truth for the procedural vegetation template meshes.
// Consumed by BOTH the raster renderers (GrassRenderer/ShrubRenderer/TreeRenderer
// upload these into their vertex buffers) and the ray-tracing path
// (RenderSystem::CollectVegetationRTInstances mirrors them into device-address
// buffers for BLAS builds). Any silhouette change made here reaches raster and
// RT together; never fork these shapes per consumer.
//
// Layout: 5 floats per vertex (position xyz + uv), no normals — the raster
// shaders compute stylized normals and RT derives face normals. Instanced
// thousands of times, so keep vertex counts small.
namespace VegTemplates {

struct VegVertex {
    f32 px, py, pz;
    f32 u, v;
};

// Grass blade: base at origin, tip at y=1, width 1 at base (instance scale
// applies bladeWidth/bladeHeight). UV.y is the height fraction the shaders use
// for color ramps and wind weighting.
void BuildGrassBlade(std::vector<VegVertex>& outVerts, std::vector<u32>& outIndices);

// Shrub: crossed vertical quads forming a star, unit height, unit width.
void BuildShrub(std::vector<VegVertex>& outVerts, std::vector<u32>& outIndices);

// Tree: tapered trunk quads (uv.y < 0.5) + crossed canopy quads (uv.y >= 0.5).
// Trunk spans y 0..1 (instance/tree params scale to trunkHeight), canopy quads
// span y 0.5..1.5 around a unit radius; tree.vert and the RT bake apply
// trunkWidth/trunkHeight/canopyRadius/canopyOffset.
void BuildTree(std::vector<VegVertex>& outVerts, std::vector<u32>& outIndices);

} // namespace VegTemplates
} // namespace Effects
} // namespace Enjin
