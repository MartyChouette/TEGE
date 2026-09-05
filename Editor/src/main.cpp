#include "Enjin/Core/Application.h"
#include "Enjin/Core/Version.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Debug/CrashHandler.h"
#include "Enjin/Platform/Input.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Systems/RenderSystem.h"
#include "Enjin/Renderer/Vulkan/VulkanRenderer.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Renderer/Camera.h"
#include "Enjin/Renderer/CameraController.h"
#include "Enjin/Editor/EditorLayer.h"
#include "Enjin/Audio/AudioSystem.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include "Enjin/Assets/SceneImporter.h"
#include "Enjin/Scene/SceneSerializer.h"
#include "Enjin/Build/BuildPipeline.h"
#include "Enjin/Build/BuildReport.h"
#include <memory>
#if defined(_WIN32)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    #ifdef CreateWindow
        #undef CreateWindow
    #endif
#else
    #include <unistd.h>
    #include <cstdio>
#endif

// Crash context — file-scope pointers for function-pointer providers
static Enjin::Renderer::VulkanRenderer* s_CrashRenderer = nullptr;
static Enjin::ECS::World* s_CrashWorld = nullptr;
static Enjin::Editor::EditorLayer* s_CrashEditor = nullptr;
static char s_GPUNameBuf[256] = {};

// Editor application
class EditorApplication : public Enjin::Application {
public:
    void Initialize() override {
        ENJIN_LOG_INFO(Editor, "Enjin Editor starting...");

        // Initialize Vulkan renderer
        m_Renderer = std::make_unique<Enjin::Renderer::VulkanRenderer>();
        if (!m_Renderer->Initialize(GetWindow())) {
            ENJIN_LOG_FATAL(Editor, "Failed to initialize Vulkan renderer");
            m_Renderer.reset();
            return;
        }

        // Register window resize callback to proactively trigger swapchain recreation
        GetWindow()->SetResizeCallback([this](Enjin::u32, Enjin::u32) {
            if (m_Renderer) {
                m_Renderer->SetFramebufferResized(true);
            }
        });

        // Setup camera
        m_Camera = std::make_unique<Enjin::Renderer::Camera>();
        m_Camera->SetPerspective(45.0f, 16.0f / 9.0f, 0.1f, 1000.0f);
        m_Camera->SetPosition(Enjin::Math::Vector3(0.0f, 2.5f, 7.0f));

        // Setup camera controller
        m_CameraController = std::make_unique<Enjin::Renderer::CameraController>(m_Camera.get());
        m_CameraController->SetMoveSpeed(5.0f);
        m_CameraController->SetLookSensitivity(0.15f);

        // Create ECS world
        m_World = std::make_unique<Enjin::ECS::World>();

        // Setup render system
        m_RenderSystem = m_World->RegisterSystem<Enjin::ECS::RenderSystem>(m_World.get(), m_Renderer.get());
        m_RenderSystem->SetCamera(m_Camera.get());
        m_RenderSystem->SetEditorMode(true);
        m_RenderSystem->Initialize();

        // Setup editor UI
        m_EditorLayer = std::make_unique<Enjin::Editor::EditorLayer>();
        if (!m_EditorLayer->Initialize(GetWindow(), m_Renderer.get())) {
            ENJIN_LOG_ERROR(Editor, "Failed to initialize editor layer");
            m_EditorLayer.reset();
        } else {
            m_EditorLayer->SetWorld(m_World.get());
            m_EditorLayer->SetCamera(m_Camera.get());
            m_EditorLayer->SetCameraController(m_CameraController.get());
            m_EditorLayer->SetRenderSystem(m_RenderSystem);
        }

        // Set up frame rate limiting callback
        // This callback is called each frame to determine the target FPS
        // While MINIMIZED the main loop skips Update entirely - this keeps the
        // MCP server pumped so AI tools work with TEGE in the tray.
        SetBackgroundTickCallback([this]() {
            if (m_EditorLayer && m_EditorLayer->GetMcpServer().IsRunning())
                m_EditorLayer->GetMcpServer().PumpMainThread();
        });

        SetTargetFPSCallback([this]() -> Enjin::f32 {
            if (!m_EditorLayer) return 0.0f;

            auto& settings = m_EditorLayer->GetEditorSettings();
            auto& playMode = m_EditorLayer->GetPlayMode();
            auto& sceneManager = m_EditorLayer->GetSceneManager();

            // MCP requests waiting? Bypass every unfocused/idle throttle so AI
            // tools stay responsive while TEGE is in the background (Marty
            // 2026-08-30: MCP went dormant whenever the editor lost focus).
            if (m_EditorLayer->GetMcpServer().IsRunning() &&
                m_EditorLayer->GetMcpServer().HasPendingRequests()) {
                return 0.0f;
            }

            // Check if in play mode
            // NOTE: During play mode, the main loop runs uncapped.
            // Game View frame rate is controlled separately via RenderOffscreen().
            // The GameFrameSettings.targetFrameRate is only used for exported builds.
            if (playMode.IsPlaying()) {
                auto& gameSettings = sceneManager.GetGameFrameSettings();

                // Handle background behavior when unfocused during play mode
                if (!IsFocused()) {
                    switch (gameSettings.backgroundBehavior) {
                        case Enjin::Scene::BackgroundBehavior::Pause:
                            // Return a very low FPS when paused
                            return 5.0f;
                        case Enjin::Scene::BackgroundBehavior::ReduceTo30:
                            return 30.0f;
                        case Enjin::Scene::BackgroundBehavior::RunNormally:
                        default:
                            break;
                    }
                }

                // Run uncapped during play mode - Game View FPS is controlled
                // by the dropdown in the Game View panel via RenderOffscreen()
                return 0.0f;
            }

            // Editor mode - use editor frame settings
            // Check unfocused reduction
            if (!IsFocused() && settings.reduceFrameRateWhenUnfocused) {
                return static_cast<Enjin::f32>(settings.unfocusedFrameRate);
            }

            // Check idle reduction
            if (settings.reduceFrameRateWhenIdle && GetIdleTime() > settings.idleTimeoutSeconds) {
                return static_cast<Enjin::f32>(settings.idleFrameRate);
            }

            // Return editor frame rate limit (0 = uncapped)
            Enjin::u32 limitVal = static_cast<Enjin::u32>(settings.editorFrameRateLimit);
            return limitVal > 0 ? static_cast<Enjin::f32>(limitVal) : 0.0f;
        });

        // Register crash context providers for enriched crash reports
        s_CrashRenderer = m_Renderer.get();
        s_CrashWorld = m_World.get();
        s_CrashEditor = m_EditorLayer.get();

        // Cache GPU name once (queried from Vulkan physical device)
        if (m_Renderer && m_Renderer->GetContext()) {
            VkPhysicalDeviceProperties props = {};
            vkGetPhysicalDeviceProperties(m_Renderer->GetContext()->GetPhysicalDevice(), &props);
            snprintf(s_GPUNameBuf, sizeof(s_GPUNameBuf), "%s", props.deviceName);
        }

        Enjin::Debug::CrashContext ctx;
        ctx.engineVersion = []() -> const char* { return ENJIN_VERSION_STRING; };
        ctx.gpuName = []() -> const char* { return s_GPUNameBuf; };
        ctx.sceneName = []() -> const char* {
            if (s_CrashEditor) {
                auto& sm = s_CrashEditor->GetSceneManager();
                const auto& name = sm.GetCurrentSceneName();
                return name.empty() ? "(unsaved)" : name.c_str();
            }
            return "unknown";
        };
        ctx.entityCount = []() -> Enjin::u32 {
            if (s_CrashWorld) {
                return static_cast<Enjin::u32>(s_CrashWorld->GetEntityCount());
            }
            return 0;
        };
        Enjin::Debug::SetCrashContext(ctx);

        ENJIN_LOG_INFO(Editor, "Editor initialized - Use RMB + WASD to fly, scroll to adjust speed");
    }

    void Shutdown() override {
        ENJIN_LOG_INFO(Editor, "Enjin Editor shutting down...");

        // Clear crash context pointers before teardown
        s_CrashRenderer = nullptr;
        s_CrashWorld = nullptr;
        s_CrashEditor = nullptr;
        Enjin::Debug::SetCrashContext({});

        if (m_EditorLayer) {
            m_EditorLayer->Shutdown();
            m_EditorLayer.reset();
        }

        if (m_RenderSystem) {
            m_RenderSystem->Shutdown();
            m_RenderSystem = nullptr;
        }
        m_World.reset();
        m_CameraController.reset();
        m_Camera.reset();
        m_Renderer.reset();
    }

    void Update(Enjin::f32 deltaTime) override {
        // Update editor layer first (drives splash/hub state)
        if (m_EditorLayer) {
            m_EditorLayer->Update(deltaTime);
        }

        // Skip camera input when splash screen or project hub covers the screen
        if (m_EditorLayer && m_EditorLayer->IsShowingFullscreenOverlay()) {
            return;
        }

        // Update camera controller only when the editor viewport is hovered
        // (or when already captured from a prior RMB press in the viewport)
        if (m_CameraController) {
            bool viewportHovered = m_EditorLayer && m_EditorLayer->IsEditorViewportHovered();
            bool alreadyCaptured = m_CameraController->IsMouseCaptured();
            if (viewportHovered || alreadyCaptured) {
                m_CameraController->Update(deltaTime);
            }
        }
    }

    void Render() override {
        if (!m_Renderer) {
            return;
        }

        if (!m_Renderer->BeginFrameVulkan()) {
            m_FrameFailCount++;
            if (m_Renderer->IsDeviceLost()) {
                ENJIN_LOG_FATAL(Editor, "GPU device lost — shutting down. Please restart the application.");
                RequestShutdown();
                return;
            }
            if (m_FrameFailCount % 60 == 1) {
                ENJIN_LOG_WARN(Editor, "BeginFrame failed (total failures: %u)", m_FrameFailCount);
            }
            return;
        }

        m_FrameFailCount = 0;

        // Update editor camera aspect ratio.
        // The editor/scene view camera is always perspective — it's the editor's
        // navigation camera, not the game camera. 2D game cameras are separate
        // entities with CameraComponent (used by Game View, not Scene View).
        auto extent = m_Renderer->GetSwapchainExtent();
        if (extent.width > 0 && extent.height > 0 && m_Camera) {
            Enjin::f32 aspect = static_cast<Enjin::f32>(extent.width) / static_cast<Enjin::f32>(extent.height);
            m_Camera->SetPerspective(45.0f, aspect, 0.1f, 1000.0f);
        }

        // Flush deferred changes (skybox config, pipeline recreation) before
        // any rendering commands that might reference old GPU resources
        if (m_RenderSystem) {
            m_RenderSystem->FlushPendingChanges();
        }

        // Resize render targets BEFORE command buffer recording.
        // This avoids destroying/recreating Vulkan resources while a command
        // buffer is in recording state, which crashes with Vulkan layer hooks
        // (OBS game capture, RenderDoc, etc.) that hold resource references.
        if (m_EditorLayer) {
            m_EditorLayer->PrepareRenderTargets();
        }

        // Flush AGAIN: PrepareRenderTargets' render-pass-change heal requests a
        // deferred pipeline recreation. Processing it here — while the command
        // buffer is still empty — means the recreated pipelines are used THIS
        // frame. Left pending, the frame draws with pipelines built against the
        // destroyed render pass, and the mid-frame fallback flush that used to
        // pick it up destroyed descriptor sets already bound in the recording
        // command buffer (the window-resize crash).
        if (m_RenderSystem) {
            m_RenderSystem->FlushPendingChanges();
        }

        VkCommandBuffer cmd = m_Renderer->GetCurrentCommandBuffer();

        // Render offscreen targets (before main render pass)
        if (m_EditorLayer && cmd != VK_NULL_HANDLE) {
            m_EditorLayer->RenderOffscreen(cmd);
        }

        // Render scene (starts main render pass)
        if (m_World) {
            m_World->Update(0.0f);
        }

        // Render editor UI (within main render pass)
        if (m_EditorLayer && cmd != VK_NULL_HANDLE) {
            m_EditorLayer->Render(cmd);
        }

        m_Renderer->EndFrame();
    }

private:
    std::unique_ptr<Enjin::Renderer::VulkanRenderer> m_Renderer;
    std::unique_ptr<Enjin::Renderer::Camera> m_Camera;
    std::unique_ptr<Enjin::Renderer::CameraController> m_CameraController;
    std::unique_ptr<Enjin::ECS::World> m_World;
    std::unique_ptr<Enjin::Editor::EditorLayer> m_EditorLayer;
    Enjin::ECS::RenderSystem* m_RenderSystem = nullptr;
    Enjin::u32 m_FrameFailCount = 0;
};

Enjin::Application* CreateApplication() {
    return new EditorApplication();
}

// Entry point - Engine owns this
int main(int argc, char* argv[]) {
    // If launched with a .enjinproject file path (e.g., double-click in Explorer),
    // store it so the editor opens directly into that project.
    if (argc > 1 && argv[1]) {
        std::string arg = argv[1];
        if (arg.find(".enjinproject") != std::string::npos ||
            arg.find(".enjin") != std::string::npos) {
            Enjin::Editor::EditorLayer::s_LaunchProjectPath = arg;
        }
    }
    // --play (any position): auto-enter play mode once the launch project loads.
    // --compute-skinning: force ADR-0002 compute skinning on at boot.
    // --golden <basePath> [--golden-frames N]: capture the game view after N
    //   frames as <basePath>.png/.ppm and exit (golden-image harness).
    // For automated probes (validation runs) — not user-facing.
    for (int i = 1; i < argc; i++) {
        if (!argv[i]) continue;
        std::string flag = argv[i];
        if (flag == "--play") {
            Enjin::Editor::EditorLayer::s_AutoPlayOnLaunch = true;
            Enjin::Editor::EditorLayer::s_AutoPlayRequested = true;
        } else if (flag == "--play-cycle" && i + 1 < argc && argv[i + 1]) {
            Enjin::Editor::EditorLayer::s_PlayCycleFrames = std::atoi(argv[++i]);
            // Optional second number = stop after N cycles and exit with a code
            // (T4 stress: memory-bounded pass/fail). Omitted = cycle forever.
            if (i + 1 < argc && argv[i + 1] && argv[i + 1][0] != '-') {
                Enjin::Editor::EditorLayer::s_PlayCycleMax = std::atoi(argv[++i]);
            }
        } else if (flag == "--compute-skinning") {
            Enjin::Editor::EditorLayer::s_ComputeSkinningOnLaunch = true;
        } else if (flag == "--golden" && i + 1 < argc && argv[i + 1]) {
            Enjin::Editor::EditorLayer::s_GoldenCapturePath = argv[++i];
        } else if (flag == "--golden-frames" && i + 1 < argc && argv[i + 1]) {
            Enjin::Editor::EditorLayer::s_GoldenCaptureFrame = std::atoi(argv[++i]);
        } else if (flag == "--export-templates" && i + 1 < argc && argv[i + 1]) {
            // Run every built-in template generator and write each out as a
            // project folder, so templates can become data instead of code.
            Enjin::Editor::EditorLayer::s_ExportTemplatesDir = argv[++i];
            Enjin::Editor::EditorLayer::s_ExportTemplateIndex = 0;
        }
    }

    // --build-web <project.enjinproject> [outDir]: HEADLESS. Pack the given project
    // into <outDir>/game.enjpak (default outDir = web-demo) and exit — no window, no
    // GPU. The web player fetches game.enjpak, so this is THE "build my game for web"
    // step: it always packs the project you pass, so the web build can never serve
    // stale content again. Uses Desktop packaging (pak only, no slow emscripten rebuild).
    for (int i = 1; i < argc; i++) {
        if (argv[i] && std::string(argv[i]) == "--build-web" && i + 1 < argc && argv[i + 1]) {
            std::string projectPath = argv[i + 1];
            std::string outDir = (i + 2 < argc && argv[i + 2] && argv[i + 2][0] != '-')
                                     ? argv[i + 2] : std::string("web-demo");
            Enjin::Build::BuildConfig cfg;
            cfg.projectPath = projectPath;
            cfg.outputDir = outDir;
            cfg.target = Enjin::Build::BuildTargetPlatform::Desktop;   // pak only
            cfg.packagingMode = Enjin::Build::PackagingMode::Packed;
            // The pak is all that is asked for here; the web player is copied
            // in below, so the pipeline must not demand a runtime it was never
            // told to produce.
            cfg.assetsOnly = true;
            Enjin::Build::BuildPipeline pipeline;
            pipeline.SetProgressCallback([](const std::string& phase, float p) {
                std::cout << "[build-web] " << phase << " " << int(p * 100.0f) << "%\n";
            });
            std::cout << "[build-web] packing '" << projectPath << "' -> "
                      << outDir << "/game.enjpak\n";
            Enjin::Build::BuildResult r = pipeline.Execute(cfg);
            if (!r.success) {
                // Print WHY. This said "see messages above" while printing only
                // progress percentages, so a failed pack gave the user nothing to
                // act on - the actual reason sat unread in the result.
                for (const auto& m : r.messages) {
                    if (m.severity != Enjin::Build::MessageSeverity::Error) continue;
                    std::cout << "[build-web] error: " << m.text << "\n";
                }
                std::cout << "[build-web] FAILED to pack the project\n";
                return 1;
            }
            for (const auto& m : r.messages) {
                if (m.severity == Enjin::Build::MessageSeverity::Warning)
                    std::cout << "[build-web] warning: " << m.text << "\n";
            }
            std::cout << "[build-web] packed " << r.filesPacked << " files -> "
                      << outDir << "/game.enjpak\n";

            // A web build is not done until the web PLAYER (wasm) is next to the pak.
            // Reuse the already-built engine from build-web/bin — do NOT silently ship
            // a pak with no player. If the engine isn't built, say exactly what to run.
            namespace fs = std::filesystem;
            bool haveWasm = false;
            for (const char* f : {"EnjinPlayer.js", "EnjinPlayer.wasm"}) {
                fs::path src = fs::path("build-web") / "bin" / f;
                std::error_code ec;
                if (fs::exists(src, ec)) {
                    fs::copy_file(src, fs::path(outDir) / f, fs::copy_options::overwrite_existing, ec);
                    if (!ec) haveWasm = true;
                }
            }
            if (haveWasm) {
                std::cout << "[build-web] DONE: web build ready in " << outDir
                          << " (pak + player). Serve it and reload.\n";
            } else {
                std::cout << "[build-web] pak is ready, but NO web engine build was found at "
                             "build-web/bin/EnjinPlayer.wasm.\n"
                             "[build-web] Build the web engine once, then re-run this:\n"
                             "[build-web]   emcmake cmake -B build-web -S . -DENJIN_PLATFORM_WEB=ON\n"
                             "[build-web]   emmake cmake --build build-web --target EnjinPlayer\n";
            }
            return 0;
        }
    }

    // --import <modelPath> <outProjectDir>: HEADLESS. Import an FBX/glTF/OBJ into a
    // fresh scene + project and exit. Mesh geometry is written as SOURCE REFERENCES
    // (path + mesh index), so a huge model (e.g. Sponza) stays out of the scene JSON
    // and is reloaded from its file on play. This is the no-UI way to bring a model in.
    for (int i = 1; i < argc; i++) {
        if (argv[i] && std::string(argv[i]) == "--import" && i + 2 < argc && argv[i + 1] && argv[i + 2]) {
            namespace fs = std::filesystem;
            std::string model = argv[i + 1];
            std::string outDir = argv[i + 2];
            std::error_code ec; fs::create_directories(fs::path(outDir) / "scenes", ec);
            Enjin::ECS::World world;
            Enjin::Assets::ImportResult r = Enjin::Assets::SceneImporter::Import(model, &world);
            if (!r.success) {
                std::cout << "[import] FAILED: " << r.errorMessage << "\n";
                return 1;
            }
            std::cout << "[import] " << r.meshCount << " meshes, " << r.totalVertexCount
                      << " verts, " << r.entities.size() << " entities from " << model << "\n";
            // --inline (any later arg): bake full geometry into the scene instead of a
            // source reference. Needed for multi-material meshes (ref resolution only
            // restores one submesh); costs a bigger scene file. Refs stay default for
            // huge single-material models (Sponza).
            bool inlineGeom = false;
            for (int k = i + 3; k < argc; k++) if (argv[k] && std::string(argv[k]) == "--inline") inlineGeom = true;
            Enjin::Scene::SceneSerializer ser(&world);
            Enjin::Scene::SerializationOptions so; so.useMeshReferences = !inlineGeom;
            std::string sceneJson = ser.SaveToString(so);
            { std::ofstream f(fs::path(outDir) / "scenes" / "Main.enjin"); f << sceneJson; }
            std::string name = fs::path(outDir).filename().string();
            { std::ofstream f(fs::path(outDir) / (name + ".enjinproject"));
              f << "{\n \"name\": \"" << name << "\",\n \"version\": \"1.0\",\n"
                   " \"scenes\": [\n  { \"path\": \"scenes/Main.enjin\", \"buildIndex\": 0, \"isStartScene\": true }\n ]\n}\n"; }
            std::cout << "[import] wrote " << outDir << "/scenes/Main.enjin (mesh refs -> "
                      << model << ")\n";
            return 0;
        }
    }

    Enjin::Application* app = CreateApplication();
    int result = app->Run();
    delete app;
    // T4 play-cycle stress: surface the probe's pass/fail as the process code
    if (Enjin::Editor::EditorLayer::s_PlayCycleMax > 0 && result == 0) {
        result = Enjin::Editor::EditorLayer::s_PlayCycleExitCode;
    }

    std::cout << "Application exited with code " << result << "." << std::endl;
#if !defined(_WIN32)
    // Only pause when launched from an interactive terminal.
    if (isatty(fileno(stdin))) {
        std::cout << "Press Enter to close..." << std::endl;
        std::cin.get();
    }
#endif

    return result;
}

// Windows GUI subsystem entry point — forwards to main().
// This prevents the black console window from appearing on launch.
// Linux/Mac don't need this — they don't spawn consoles for GUI apps.
#if defined(_WIN32)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR lpCmdLine, int) {
    // Forward command line to main so file associations work
    return main(__argc, __argv);
}
#endif
