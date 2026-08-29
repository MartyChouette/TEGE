# API Reference

## Core Systems

### Memory Management

```cpp
// Allocators
StackAllocator stack(1024 * 1024); // 1MB stack
void* ptr = stack.Allocate(256);
stack.FreeToMarker(marker);

PoolAllocator pool(sizeof(MyClass), 100); // Pool of 100 objects
MyClass* obj = static_cast<MyClass*>(pool.Allocate(sizeof(MyClass)));

LinearAllocator linear(1024 * 1024); // 1MB linear
void* data = linear.Allocate(512);
linear.Reset(); // Free all
```

### Math Library

```cpp
// Vectors
Math::Vector3 pos(1.0f, 2.0f, 3.0f);
Math::Vector3 normalized = pos.Normalized();
f32 length = pos.Length();
f32 dot = pos.Dot(other);

// Matrices
Math::Matrix4 transform = Math::Matrix4::Translation(pos) *
                          Math::Matrix4::Rotation(axis, angle) *
                          Math::Matrix4::Scale(scale);

// Quaternions
Math::Quaternion rot = Math::Quaternion::FromEuler(euler);
Math::Matrix4 rotMat = rot.ToMatrix();
```

### Logging

```cpp
ENJIN_LOG_INFO(Core, "Engine initialized");
ENJIN_LOG_WARN(Renderer, "Texture not found: %s", path);
ENJIN_LOG_ERROR(Physics, "Collision detection failed");
ENJIN_LOG_FATAL(Core, "Critical error: %s", message);
```

## Rendering Systems

### VulkanRenderer

```cpp
VulkanRenderer renderer;
renderer.Initialize(window);
renderer.BeginFrame();
// ... render ...
renderer.EndFrame();
```

### RenderPipeline

```cpp
RenderPipeline pipeline(&renderer);

// Register hooks
pipeline->RegisterHook(RenderEventType::PreDraw, [](RenderEvent& event) {
    // Custom logic
});

// Materials
u32 materialId = pipeline->RegisterMaterial({
    .name = "PBR",
    .shaderPath = "shaders/pbr.frag",
    .floatParams = {{"metallic", 0.5f}}
});

// Pipeline state
PipelineState state;
state.lineWidth = 2.0f;
pipeline->SetPipelineState(state);
```

### Rendering Techniques

```cpp
RenderingTechniqueManager techniques;
techniques.RegisterTechnique(std::make_unique<ForwardRendering>());
techniques.SwitchTechnique("ForwardRendering");
techniques.Render(deltaTime);
```

### GPU Culling

```cpp
GPUCullingSystem culling(context);
culling.Initialize();

std::vector<CullableObject> objects;
culling.SubmitObjects(objects);

VkBuffer indirectBuffer;
u32 drawCount;
culling.ExecuteCulling(view, proj, cmd, indirectBuffer, drawCount);
```

### Bindless Resources

```cpp
BindlessResourceManager bindless(context);
bindless.Initialize();

BindlessHandle handle = bindless.RegisterTexture(imageView, sampler);
bindless.UpdateDescriptorSet();

// In shader: texture(textures[handle], uv)
```

## ECS System

```cpp
World world;

// Create entity
Entity entity = world.CreateEntity();

// Add components
TransformComponent& transform = world.AddComponent<TransformComponent>(entity);
transform.position = Math::Vector3(0, 0, 0);

MeshComponent& mesh = world.AddComponent<MeshComponent>(entity);
mesh.vertices = { /* ... */ };

// Register systems
RenderSystem* render = world.RegisterSystem<RenderSystem>(&world, &renderer);

// Update
world.Update(deltaTime);
```

## Animation System

```cpp
// Skeletal animation is set up automatically when importing skinned glTF models.
// For manual control:

// Access animator on a skinned entity
AnimatorComponent* animComp = world.GetComponent<AnimatorComponent>(entity);
if (animComp) {
    // Play an animation by name
    animComp->animator.Play("Walk");

    // Cross-fade between animations
    animComp->animator.CrossFade("Run", 0.3f);

    // Adjust playback speed
    animComp->animator.SetSpeed(1.5f);

    // Query state
    bool playing = animComp->animator.IsPlaying();
    bool blending = animComp->animator.IsBlending();
    f32 progress = animComp->animator.GetNormalizedTime();

    // Get bone world transform (for attachments, IK)
    Math::Matrix4 handTransform = animComp->animator.GetBoneWorldTransform("Hand_R");

    // Override bone rotation (procedural animation, IK)
    animComp->animator.SetBoneLocalRotation("Head", lookAtRotation);
}

// State machine for animation transitions
AnimatorComponent* animComp = world.GetComponent<AnimatorComponent>(entity);
if (animComp) {
    auto& sm = animComp->stateMachine;
    sm.SetDefaultState("Idle");

    Animation::AnimationState idleState;
    idleState.name = "Idle";
    idleState.animationName = "Idle";
    sm.AddState(idleState);

    Animation::AnimationState walkState;
    walkState.name = "Walk";
    walkState.animationName = "Walk";
    sm.AddState(walkState);

    Animation::AnimationTransition toWalk;
    toWalk.fromState = "Idle";
    toWalk.toState = "Walk";
    toWalk.blendTime = 0.2f;
    sm.AddTransition(toWalk);

    // Drive transitions with parameters
    sm.SetBool("isWalking", true);
}
```

## Time System

```cpp
TimeOfDay time;
time.SetTime(12.0f); // Noon
time.SetDayLength(300.0f); // 5 min = 24 hours
time.Update(deltaTime);

Math::Vector3 sunDir = time.GetSunDirection();
Math::Vector3 sunColor = time.GetSunColor();
Math::Vector4 skyColor = time.GetSkyColor();
```

## Weather System

```cpp
WeatherSystem weather;
weather.Initialize();
weather.SetWeather(WeatherType::Rain, 0.8f);
weather.SetWindSpeed(5.0f);
weather.Update(deltaTime);

f32 fogDensity = weather.GetFogDensity();
```

## Physics System

```cpp
PhysicsWorld physics;
physics.Initialize();
physics.SetGravity(Math::Vector3(0, -9.81f, 0));

auto body = std::make_shared<RigidBody>();
body->SetPosition(Math::Vector3(0, 10, 0));
physics.AddRigidBody(body);
physics.Step(deltaTime);
```

## Water System

```cpp
WaterRenderer water;
water.Initialize(&renderer);
water.SetWaterLevel(0.0f);
water.SetWaveAmplitude(0.5f);
water.Render(deltaTime, cameraPosition);
```

## GUI System

```cpp
ShaderGUI gui;
gui.Initialize();
gui.RegisterMaterial(material);
gui.ShowMaterialEditor(true);
gui.Render();
```

## Ray Tracing System

### RTCapabilities

```cpp
// Query RT hardware support (before logical device creation)
RTCapabilities caps = RTCapabilities::Query(physicalDevice);
if (caps.supported) {
    // All required extensions available
    // caps.maxRayRecursionDepth, caps.shaderGroupHandleSize, etc.
}

// Get required device extensions
const auto& extensions = RTCapabilities::GetRequiredExtensions();
```

### AccelerationStructureManager

```cpp
AccelerationStructureManager asManager(context);
asManager.Initialize();

// Register a mesh (returns BLAS ID, deduplicates by hash)
u32 blasId = asManager.RegisterMesh(meshHash,
    vertexAddress, vertexCount, vertexStride,
    indexAddress, indexCount);

// Build pending BLAS structures
asManager.FlushPendingBLASBuilds(commandBuffer);

// Per-frame: add instances and rebuild TLAS
asManager.ResetInstances();
asManager.AddInstance(blasId, modelMatrix, customIndex, mask, sbtOffset, flags);
asManager.BuildTLAS(commandBuffer, transformsOnly);

// Access TLAS
VkAccelerationStructureKHR tlas = asManager.GetTLAS();
bool valid = asManager.HasValidTLAS();
```

### RT Effects

```cpp
// Each effect follows the same pattern:
RTShadows shadows(context);
shadows.Initialize(width, height);
shadows.Dispatch(commandBuffer, config);
shadows.Shutdown();

// Config structs per effect:
RTShadowConfig shadowConfig;
shadowConfig.enabled = true;
shadowConfig.maxDistance = 100.0f;
shadowConfig.shadowRadius = 0.01f;

RTReflectionConfig reflectConfig;
reflectConfig.enabled = true;
reflectConfig.maxDistance = 50.0f;
reflectConfig.roughnessThreshold = 0.5f;

RTAOConfig aoConfig;
aoConfig.enabled = true;
aoConfig.radius = 2.0f;
aoConfig.power = 1.0f;

RTGIConfig giConfig;
giConfig.enabled = true;
giConfig.bounces = 2;
giConfig.intensity = 1.0f;
```

### Path Tracer

```cpp
PathTracer pathTracer(context);
pathTracer.Initialize(width, height);

// Progressive rendering (1 SPP per dispatch)
pathTracer.Dispatch(commandBuffer, config);

// Query convergence
u32 currentSPP = pathTracer.GetCurrentSPP();
bool converged = pathTracer.IsConverged();

// Reset accumulation (on camera/scene change)
pathTracer.ResetAccumulation();
```

### SVGFDenoiser

```cpp
SVGFDenoiser denoiser(context);
denoiser.Initialize(width, height);

// 3-pass denoise
denoiser.Denoise(commandBuffer, noisyInput, denoisedOutput,
    depthImage, normalImage, motionVectorImage);

// Configure
denoiser.SetTemporalAlpha(0.05f);
denoiser.SetATrousIterations(5);
denoiser.ResetHistory();  // On scene change
```

## Feedback & Bug Reporting System

### FeedbackManager

```cpp
#include "Enjin/Editor/FeedbackSystem.h"
using namespace Enjin::Editor;

FeedbackManager manager;

// Load/save persisted reports (JSON in %APPDATA%/enjin/feedback/)
manager.LoadAll();
manager.SaveAll();

// Create a bug report
BugReport& report = manager.CreateBugReport();
report.title = "Crash on load";
report.type = ReportType::Crash;
report.severity = ReportSeverity::Critical;
report.description = "Engine crashes when loading scene with >1000 entities";
report.stepsToReproduce = "1. Create scene\n2. Add 1001 entities\n3. Save and reload";
report.expectedBehavior = "Scene loads normally";
report.actualBehavior = "Application crashes with access violation";
manager.SaveBugReport(report);

// Auto-capture diagnostics (engine version, GPU, RAM, FPS, scene state)
DiagnosticSnapshot snap = DiagnosticSnapshot::Capture(
    perfMetrics, fps, frameTimeMs, entityCount,
    scenePath, consoleLog, selectedCount);

// Create feedback
FeedbackEntry& feedback = manager.CreateFeedback();
feedback.title = "Add dark mode to asset browser";
feedback.type = FeedbackType::FeatureRequest;
feedback.priority = FeedbackPriority::Medium;
feedback.satisfaction = SatisfactionRating::Good;
manager.SaveFeedback(feedback);

// Search and filter
auto results = manager.SearchBugReports("crash");
auto filtered = manager.FilterBugReports(
    static_cast<i32>(ReportStatus::Draft),
    static_cast<i32>(ReportSeverity::Critical));

// Export
manager.ExportBugReportAsJson(report.id, "report.json");
manager.ExportAllAsJson("all_reports.json");

// Remote submission via HTTPClient
manager.SubmitBugReport(report.id, "https://api.example.com/bugs");
manager.SubmitFeedback(feedback.id, "https://api.example.com/feedback");
```

## Vector Drawing Editor

```cpp
#include "Enjin/Editor/VectorDrawingEditor.h"
using namespace Enjin::Editor;

// VectorDrawingEditor is an EditorPanel (1<<29)
// Supports 7 shape types: Line, Rect, Ellipse, Pen, Bezier, Star, Polygon
// 8 tools: Select, Line, Rectangle, Ellipse, Pen, Bezier, Star, Polygon
// Features: layers, undo/redo (50 levels), SVG export, snap-to-grid, zoom/pan
// Symbol library integration for timeline authoring
```

## HTML5 Export

```cpp
#include "Enjin/Build/HTML5Exporter.h"
using namespace Enjin::Build;

HTML5Exporter exporter;
HTML5ExportConfig config;
config.title = "My Game";
config.width = 800;
config.height = 600;
config.outputDir = "export/html5";

// Generates: index.html, preloader.js, style.css
// Includes: responsive scaling, fullscreen, click-to-play audio
exporter.Export(config);

// Get embed code (iframe)
std::string embedCode = exporter.GetEmbedCode();
```

## Complete API Documentation

See individual header files for detailed API documentation:
- All public methods are documented
- Parameters explained
- Usage examples included
- Performance notes provided
