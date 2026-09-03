#pragma once
#include "Enjin/Platform/Types.h"

// One source of truth for the web scene render target's sample count.
//
// Every pipeline that draws into the web scene pass must declare the SAME
// sample count as the scene color + depth textures, or WebGPU rejects the draw
// with "Incompatible sample count" on set_pipeline -- an uncaptured error that
// cascades into invalid buffers/bind groups and a black canvas.
//
// This constant exists because that is exactly what happened: MSAA was turned
// off by editing RenderSystem.cpp alone, which left the two pipelines that live
// in their own files (WebGPUVegetationSystem "veg-scene" and
// WebGPUParticleSystem "particle-draw-scene") at 4x. Every demo that draws
// grass, trees or particles went black. Anything that creates a scene-pass
// pipeline or scene target must read this rather than hardcode a literal.
//
// WebGPU supports ONLY 1 or 4 -- there is no 2x. Raising this back to 4 also
// requires restoring the MSAA color buffer and the resolve step in
// RenderSystem's web target setup.
namespace Enjin::Renderer {

inline constexpr u32 kWebSceneSampleCount = 1;

} // namespace Enjin::Renderer
