#include "EnjinTest.h"
#include "Enjin/Renderer/RayTracing/RTCapabilities.h"
#include "Enjin/Renderer/RayTracing/RTShadows.h"
#include "Enjin/Renderer/RayTracing/RTReflections.h"
#include "Enjin/Renderer/RayTracing/RTAmbientOcclusion.h"
#include "Enjin/Renderer/RayTracing/RTGlobalIllumination.h"
#include "Enjin/Renderer/RayTracing/PathTracer.h"
#include "Enjin/Renderer/RayTracing/SVGFDenoiser.h"

using namespace Enjin;
using namespace Enjin::Renderer;

// ===========================================================================
// RTCapabilities Defaults
// ===========================================================================

ENJIN_TEST(Capabilities, Defaults) {
    RTCapabilities caps;
    ENJIN_EXPECT_FALSE(caps.supported);
    ENJIN_EXPECT_FALSE(caps.accelerationStructure);
    ENJIN_EXPECT_FALSE(caps.rayTracingPipeline);
    ENJIN_EXPECT_FALSE(caps.deferredHostOperations);
    ENJIN_EXPECT_FALSE(caps.bufferDeviceAddress);
    ENJIN_EXPECT_EQ(caps.maxRayRecursionDepth, 0u);
    ENJIN_EXPECT_EQ(caps.shaderGroupHandleSize, 0u);
    ENJIN_EXPECT_EQ(caps.shaderGroupHandleAlignment, 0u);
    ENJIN_EXPECT_EQ(caps.shaderGroupBaseAlignment, 0u);
    ENJIN_EXPECT_EQ(caps.maxRayDispatchInvocationCount, 0u);
    ENJIN_EXPECT_EQ(caps.maxGeometryCount, (u64)0);
    ENJIN_EXPECT_EQ(caps.maxInstanceCount, (u64)0);
    ENJIN_EXPECT_EQ(caps.maxPrimitiveCount, (u64)0);
}

ENJIN_TEST(Capabilities, RequiredExtensions) {
    auto& exts = RTCapabilities::GetRequiredExtensions();
    ENJIN_EXPECT_GT(exts.size(), (size_t)0);
}

// ===========================================================================
// RT Shadow Config
// ===========================================================================

ENJIN_TEST(Shadow, ConfigDefaults) {
    RTShadowConfig cfg;
    ENJIN_EXPECT_FALSE(cfg.enabled);
    ENJIN_EXPECT_FLOAT_EQ(cfg.maxDistance, 100.0f);
    ENJIN_EXPECT_FLOAT_NEAR(cfg.radius, 0.01f, 0.001f);
}

// ===========================================================================
// RT Reflection Config
// ===========================================================================

ENJIN_TEST(Reflection, ConfigDefaults) {
    RTReflectionConfig cfg;
    ENJIN_EXPECT_FALSE(cfg.enabled);
    ENJIN_EXPECT_FLOAT_EQ(cfg.maxDistance, 50.0f);
    ENJIN_EXPECT_FLOAT_EQ(cfg.roughnessThreshold, 0.5f);
}

// ===========================================================================
// RT AO Config
// ===========================================================================

ENJIN_TEST(AO, ConfigDefaults) {
    RTAOConfig cfg;
    ENJIN_EXPECT_FALSE(cfg.enabled);
    ENJIN_EXPECT_FLOAT_EQ(cfg.radius, 2.0f);
    ENJIN_EXPECT_FLOAT_EQ(cfg.power, 1.5f);
}

// ===========================================================================
// RT GI Config
// ===========================================================================

ENJIN_TEST(GI, ConfigDefaults) {
    RTGIConfig cfg;
    ENJIN_EXPECT_FALSE(cfg.enabled);
    ENJIN_EXPECT_FLOAT_EQ(cfg.maxDistance, 50.0f);
    ENJIN_EXPECT_FLOAT_EQ(cfg.intensity, 1.0f);
    ENJIN_EXPECT_EQ(cfg.bounces, 1u);
}

// ===========================================================================
// Path Tracer Config
// ===========================================================================

ENJIN_TEST(PathTracer, ConfigDefaults) {
    PathTracerConfig cfg;
    ENJIN_EXPECT_EQ(cfg.maxBounces, 4u);
    ENJIN_EXPECT_EQ(cfg.targetSPP, 1024u);
}

// ===========================================================================
// SVGF Denoiser Config
// ===========================================================================

ENJIN_TEST(SVGF, ConfigDefaults) {
    SVGFConfig cfg;
    ENJIN_EXPECT_FLOAT_NEAR(cfg.temporalAlpha, 0.05f, 0.001f);
    ENJIN_EXPECT_EQ(cfg.atrousIterations, 5u);
    ENJIN_EXPECT_FLOAT_EQ(cfg.sigmaDepth, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(cfg.sigmaNormal, 128.0f);
    ENJIN_EXPECT_FLOAT_EQ(cfg.sigmaLuminance, 4.0f);
}

ENJIN_TEST_MAIN()
