#include "Enjin/Platform/Platform.h"

#if ENJIN_PLATFORM_WEB

#include "Enjin/Renderer/WebGPU/WebGPUComputeSmokeTest.h"
#include "Enjin/Renderer/WebGPU/WebGPURenderer.h"
#include "Enjin/Renderer/WebGPU/WebGPUPipelineManager.h"
#include "Enjin/Renderer/WebGPU/WebGPUBindGroupManager.h"
#include "Enjin/Renderer/WebGPU/WebGPUBufferManager.h"
#include "Enjin/Renderer/WebGPU/WebGPUComputeEncoder.h"
#include "Enjin/Renderer/GPUBuffer.h"
#include "Enjin/Renderer/GPUShader.h"
#include "Enjin/Renderer/GPUBindGroup.h"
#include "Enjin/Renderer/GPUPipeline.h"
#include "Enjin/Logging/Log.h"

#include <webgpu/webgpu.h>
#include <cstring>

namespace Enjin::Renderer {

static constexpr u32 kSmokeCount = 256;                  // elements written by the shader
static constexpr u64 kSmokeBytes = kSmokeCount * sizeof(u32);
static WGPUBuffer g_smokeReadback = nullptr;             // kept alive until the map callback runs

// Async map completion: verify data[i] == i*2+1 for every element, log PASS/FAIL.
static void SmokeMapCallback(WGPUMapAsyncStatus status, WGPUStringView, void*, void*) {
    if (status != WGPUMapAsyncStatus_Success) {
        ENJIN_LOG_ERROR(Renderer, "[compute-smoke] FAIL: buffer map failed (status %d)",
                        static_cast<int>(status));
        return;
    }
    const u32* data = static_cast<const u32*>(
        wgpuBufferGetConstMappedRange(g_smokeReadback, 0, kSmokeBytes));
    if (!data) {
        ENJIN_LOG_ERROR(Renderer, "[compute-smoke] FAIL: mapped range is null");
        return;
    }
    u32 bad = 0, firstBadIdx = 0, firstBadVal = 0;
    for (u32 i = 0; i < kSmokeCount; ++i) {
        u32 expected = i * 2u + 1u;
        if (data[i] != expected) {
            if (bad == 0) { firstBadIdx = i; firstBadVal = data[i]; }
            ++bad;
        }
    }
    if (bad == 0) {
        ENJIN_LOG_INFO(Renderer,
            "[compute-smoke] PASS: all %u elements correct (data[i] == i*2+1). WebGPU compute works.",
            kSmokeCount);
    } else {
        ENJIN_LOG_ERROR(Renderer,
            "[compute-smoke] FAIL: %u/%u elements wrong (first: data[%u]=%u, expected %u)",
            bad, kSmokeCount, firstBadIdx, firstBadVal, firstBadIdx * 2u + 1u);
    }
    wgpuBufferUnmap(g_smokeReadback);
}

void RunWebGPUComputeSmokeTest(WebGPURenderer* renderer) {
    if (!renderer) return;
    WGPUDevice device = renderer->GetDevice();
    WGPUQueue  queue  = renderer->GetQueue();
    if (!device || !queue) {
        ENJIN_LOG_ERROR(Renderer, "[compute-smoke] no device/queue");
        return;
    }

    auto* bufMgr   = static_cast<WebGPUBufferManager*>(renderer->GetBufferManager());
    auto* shaderMgr = renderer->GetShaderManager();
    auto* bgMgr    = static_cast<WebGPUBindGroupManager*>(renderer->GetBindGroupManager());
    auto* pipeMgr  = static_cast<WebGPUPipelineManager*>(renderer->GetPipelineManager());
    if (!bufMgr || !shaderMgr || !bgMgr || !pipeMgr) {
        ENJIN_LOG_ERROR(Renderer, "[compute-smoke] missing a GPU manager");
        return;
    }

    // 1. WGSL compute shader: data[i] = i*2 + 1.
    const char* wgsl =
        "@group(0) @binding(0) var<storage, read_write> data: array<u32>;\n"
        "@compute @workgroup_size(64)\n"
        "fn main(@builtin(global_invocation_id) gid: vec3<u32>) {\n"
        "    let i = gid.x;\n"
        "    if (i < arrayLength(&data)) { data[i] = i * 2u + 1u; }\n"
        "}\n";
    GPUShaderHandle shader = shaderMgr->LoadShader(
        wgsl, std::strlen(wgsl) + 1, GPUShaderStage::Compute, "compute-smoke");
    if (!shader.IsValid()) {
        ENJIN_LOG_ERROR(Renderer, "[compute-smoke] WGSL compile failed");
        return;
    }

    // 2. Storage buffer the shader writes (Storage + CopySrc so we can read it back).
    GPUBufferDesc sdesc;
    sdesc.size  = kSmokeBytes;
    sdesc.usage = GPUBufferUsage::Storage | GPUBufferUsage::CopySrc;
    sdesc.label = "smoke-storage";
    GPUBufferHandle storage = bufMgr->CreateBuffer(sdesc);
    if (!storage.IsValid()) {
        ENJIN_LOG_ERROR(Renderer, "[compute-smoke] storage buffer create failed");
        return;
    }

    // 3. Bind group layout + bind group (one read_write storage buffer at binding 0).
    GPUBindGroupLayoutDesc ldesc;
    ldesc.entries.push_back({0, GPUBindingType::StorageBuffer, GPUShaderStage::Compute, kSmokeBytes});
    ldesc.label = "smoke-layout";
    GPUBindGroupLayoutHandle layout = bgMgr->CreateBindGroupLayout(ldesc);

    GPUBindGroupDesc gdesc;
    gdesc.layout = layout;
    gdesc.label  = "smoke-bindgroup";
    GPUBindGroupEntry entry;
    entry.binding = 0;
    entry.buffer  = storage;
    entry.bufferOffset = 0;
    entry.bufferSize   = kSmokeBytes;
    gdesc.entries.push_back(entry);
    GPUBindGroupHandle bindGroup = bgMgr->CreateBindGroup(gdesc);

    // 4. Compute pipeline (the new CreateComputePipeline path under test).
    GPUComputePipelineDesc pdesc;
    pdesc.computeShader = shader;
    pdesc.bindGroupLayouts.push_back(layout);
    pdesc.entryPoint = "main";
    pdesc.label = "smoke-pipeline";
    GPUPipelineHandle pipeline = pipeMgr->CreateComputePipeline(pdesc);
    if (!pipeline.IsValid()) {
        ENJIN_LOG_ERROR(Renderer, "[compute-smoke] compute pipeline create failed");
        return;
    }

    // 5. Mappable readback buffer (raw: the abstraction has no MapRead usage).
    WGPUBufferDescriptor rbDesc = {};
    rbDesc.size  = kSmokeBytes;
    rbDesc.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
    g_smokeReadback = wgpuDeviceCreateBuffer(device, &rbDesc);

    // 6. Encode on our own command encoder: compute pass -> dispatch -> copy -> submit.
    WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(device, nullptr);
    WGPUComputePassDescriptor cpDesc = {};
    WGPUComputePassEncoder pass = wgpuCommandEncoderBeginComputePass(enc, &cpDesc);
    {
        // Exercise the real WebGPUComputeEncoder (the new dispatch path).
        WebGPUComputeEncoder ce(renderer, pass, pipeMgr, bgMgr);
        ce.BindPipeline(pipeline);
        ce.SetBindGroup(0, bindGroup, 0, nullptr);
        ce.Dispatch((kSmokeCount + 63u) / 64u, 1, 1);
    }
    wgpuComputePassEncoderEnd(pass);
    wgpuComputePassEncoderRelease(pass);

    wgpuCommandEncoderCopyBufferToBuffer(
        enc, bufMgr->GetNativeBuffer(storage), 0, g_smokeReadback, 0, kSmokeBytes);

    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(enc, nullptr);
    wgpuQueueSubmit(queue, 1, &cmd);
    wgpuCommandBufferRelease(cmd);
    wgpuCommandEncoderRelease(enc);

    // 7. Async readback. The callback logs PASS/FAIL once the map resolves.
    WGPUBufferMapCallbackInfo cbInfo = {};
    cbInfo.mode = WGPUCallbackMode_AllowSpontaneous;
    cbInfo.callback = SmokeMapCallback;
    wgpuBufferMapAsync(g_smokeReadback, WGPUMapMode_Read, 0, kSmokeBytes, cbInfo);

    ENJIN_LOG_INFO(Renderer,
        "[compute-smoke] dispatched %u elements through the compute path; awaiting readback...",
        kSmokeCount);
}

} // namespace Enjin::Renderer

#endif // ENJIN_PLATFORM_WEB
