#pragma once

#include "Enjin/Platform/Platform.h"

#if ENJIN_PLATFORM_WEB

namespace Enjin::Renderer {

class WebGPURenderer;

// One-shot smoke test for the WebGPU GPU-compute path. Runs a trivial compute
// shader (data[i] = i*2+1) through the real abstraction: LoadShader (WGSL) ->
// CreateBindGroupLayout/CreateBindGroup -> CreateComputePipeline -> WebGPUComputeEncoder
// dispatch -> copy to a mappable buffer -> async readback that verifies every element
// and logs "[compute-smoke] PASS" or "FAIL" to the browser console.
//
// Call once after the WebGPU renderer is initialized. It does not touch the frame
// loop (uses its own command encoder), so it is safe to fire at startup. The readback
// is async: the PASS/FAIL line appears a few frames later once the map completes.
void RunWebGPUComputeSmokeTest(WebGPURenderer* renderer);

} // namespace Enjin::Renderer

#endif // ENJIN_PLATFORM_WEB
