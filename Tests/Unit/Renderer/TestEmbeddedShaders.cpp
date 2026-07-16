// Guards the embedded compute SPIR-V that makes clustered lighting work in
// shipped Player builds (parity Gap C). The GPU pipeline can't be created
// headless, but this verifies the embedded data is present and valid SPIR-V, so
// a broken/empty regeneration of ClusterComputeShaderData.h fails the suite.

#include "EnjinTest.h"
#include "Enjin/Renderer/ClusterComputeShaderData.h"
#include "Enjin/Renderer/EmbeddedComputeShaders.h"

using namespace Enjin;

// SPIR-V starts with magic 0x07230203 (little-endian bytes 03 02 23 07) and is a
// stream of 32-bit words, so the byte length is a non-trivial multiple of 4.
static bool IsValidSpirv(const unsigned char* data, unsigned long bytes) {
    return bytes >= 20 && (bytes % 4 == 0) &&
           data[0] == 0x03 && data[1] == 0x02 && data[2] == 0x23 && data[3] == 0x07;
}

ENJIN_TEST(EmbeddedShaders, ClusterBoundsIsValidSpirv) {
    ENJIN_EXPECT_TRUE(IsValidSpirv(Renderer::LightClusterBoundsComputeShaderData,
                                   Renderer::LightClusterBoundsComputeShaderDataSize));
}

ENJIN_TEST(EmbeddedShaders, ClusterAssignIsValidSpirv) {
    ENJIN_EXPECT_TRUE(IsValidSpirv(Renderer::LightClusterAssignComputeShaderData,
                                   Renderer::LightClusterAssignComputeShaderDataSize));
}

// --- ComputePipelineHelper embedded registry (_gen_compute.py) ---
// Every shader created through ComputePipelineHelper must be in the registry,
// or it silently no-ops in shipped Player builds (no shader tree on disk).

ENJIN_TEST(EmbeddedShaders, RegistryCoversAllHelperShaders) {
    const char* required[] = {
        "particle_simulate.comp",   // GPUParticleSystem
        "gpu_voxelize.comp",        // DDGIProbeSystem
        "ddgi_probe_update.comp",   // DDGIProbeSystem
        "ddgi_sample.comp",         // DDGIProbeSystem
        "volumetric_fog.comp",      // VolumetricFog
    };
    for (const char* name : required) {
        const auto* e = Renderer::FindEmbeddedComputeShader(name);
        ENJIN_ASSERT_TRUE(e != nullptr);
        ENJIN_EXPECT_TRUE(IsValidSpirv(e->data, e->size));
    }
}

ENJIN_TEST(EmbeddedShaders, RegistryRejectsUnknownAndNull) {
    ENJIN_EXPECT_TRUE(Renderer::FindEmbeddedComputeShader("not_a_shader.comp") == nullptr);
    ENJIN_EXPECT_TRUE(Renderer::FindEmbeddedComputeShader(nullptr) == nullptr);
}

ENJIN_TEST_MAIN()
