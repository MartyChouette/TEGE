// ComputeShadowCascades: the cascade fit both backends now share (WP-16).
// It moved out of the Vulkan-only ShadowMap so the web renderer could stop
// fitting one map around a box of unit cubes; these pin the properties the
// shaders rely on.
#include "EnjinTest.h"
#include "Enjin/Renderer/ShadowCascades.h"
#include "Enjin/Math/Math.h"
#include <cmath>

using namespace Enjin;

namespace {

Renderer::ShadowCascadeInput MakeInput(f32 yawRadians) {
    Renderer::ShadowCascadeInput in;
    const Math::Vector3 eye(0.0f, 2.0f, 0.0f);
    const Math::Vector3 fwd(std::sin(yawRadians), 0.0f, -std::cos(yawRadians));
    in.cameraView = Math::Matrix4::LookAt(eye, eye + fwd, Math::Vector3(0.0f, 1.0f, 0.0f));
    in.cameraProj = Math::Matrix4::Perspective(Math::Radians(60.0f), 16.0f / 9.0f, 0.1f, 500.0f);
    in.cameraNear = 0.1f;
    in.cameraFar = 500.0f;
    in.shadowDistance = 80.0f;
    in.cascadeCount = 4;
    in.resolution = 2048;
    in.lightDir = Math::Vector3(0.3f, -0.8f, 0.2f).Normalized();
    return in;
}

} // namespace

ENJIN_TEST(ShadowCascades, SplitsRiseAndEndAtTheShadowDistance) {
    const auto in = MakeInput(0.0f);
    f32 splits[4];
    Math::Matrix4 vp[4];
    Renderer::ComputeShadowCascades(in, splits, vp);
    for (int i = 1; i < 4; ++i) ENJIN_EXPECT_TRUE(splits[i] > splits[i - 1]);
    ENJIN_EXPECT_FLOAT_NEAR(splits[3], 80.0f, 0.01f);
    ENJIN_EXPECT_TRUE(splits[0] > in.cameraNear);
}

ENJIN_TEST(ShadowCascades, EachSliceLandsInsideItsCascade) {
    const auto in = MakeInput(0.4f);
    f32 splits[4];
    Math::Matrix4 vp[4];
    Renderer::ComputeShadowCascades(in, splits, vp);
    // A point on the view axis in the middle of each slice is in its cascade's
    // map, and its depth is inside 0..1
    const Math::Vector3 eye(0.0f, 2.0f, 0.0f);
    const Math::Vector3 fwd(std::sin(0.4f), 0.0f, -std::cos(0.4f));
    f32 prev = in.cameraNear;
    for (int c = 0; c < 4; ++c) {
        const Math::Vector3 p = eye + fwd * ((prev + splits[c]) * 0.5f);
        const Math::Vector4 clip = vp[c] * Math::Vector4(p.x, p.y, p.z, 1.0f);
        const Math::Vector3 ndc(clip.x / clip.w, clip.y / clip.w, clip.z / clip.w);
        ENJIN_EXPECT_TRUE(std::abs(ndc.x) < 1.0f && std::abs(ndc.y) < 1.0f);
        ENJIN_EXPECT_TRUE(ndc.z > 0.0f && ndc.z < 1.0f);
        prev = splits[c];
    }
}

ENJIN_TEST(ShadowCascades, TurningTheCameraKeepsTheTexelSize) {
    // The sphere fit: the ortho extent depends on the slice's shape, not its
    // orientation, so the texel grid does not rescale as the camera turns
    f32 s0[4], s1[4];
    Math::Matrix4 a[4], b[4];
    Renderer::ComputeShadowCascades(MakeInput(0.0f), s0, a);
    Renderer::ComputeShadowCascades(MakeInput(1.1f), s1, b);
    for (int c = 0; c < 4; ++c) {
        ENJIN_EXPECT_FLOAT_NEAR(std::abs(a[c].m[0]), std::abs(b[c].m[0]), 1e-6f);
    }
}

ENJIN_TEST_MAIN()
