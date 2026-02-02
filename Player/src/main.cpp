#include "Enjin/Core/Application.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Platform/Input.h"
#include "Enjin/Platform/Paths.h"
#include "Enjin/Platform/Window.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Systems/RenderSystem.h"
#include "Enjin/ECS/Components/Camera.h"
#include "Enjin/ECS/Components/Light.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Text.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/Renderer/Skybox.h"
#include "Enjin/Renderer/Vulkan/VulkanRenderer.h"
#include "Enjin/Renderer/Camera.h"
#include "Enjin/Renderer/CameraController.h"
#include "Enjin/Scene/SceneSerializer.h"
#include "Enjin/Scene/SceneManager.h"
#include "Enjin/Input/InputAction.h"
#include "Enjin/GUI/GameMenus.h"
#include "Enjin/GUI/ImGuiLayer.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include <imgui.h>
#include "Enjin/Effects/Weather.h"
#include "Enjin/Effects/Water.h"
#include "Enjin/Effects/Wind.h"
#include "Enjin/Effects/RetroEffects.h"
#include "Enjin/Effects/WorldTime.h"
#include "Enjin/Effects/SeasonalWeather.h"
#include "Enjin/Audio/AudioSystem.h"
#include "Enjin/Build/AssetReader.h"
#include "Enjin/Scripting/ScriptEngine.h"
#include "Enjin/Scripting/ScriptSystem.h"
#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Scripting/CoroutineScheduler.h"
#include "Enjin/Scripting/ScriptEvents.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <memory>
#include <filesystem>

namespace fs = std::filesystem;

// Standalone game player — no editor, no ImGui
class GamePlayer : public Enjin::Application {
public:
    void Initialize() override {
        ENJIN_LOG_INFO(Player, "Enjin Player starting...");

        // Open .enjpak from the same directory as the executable
        std::string exeDir = Enjin::Platform::GetExecutableDirectory();
        std::string pakPath = (fs::path(exeDir) / "game.enjpak").string();

        if (!m_AssetReader.Open(pakPath, PACK_KEY)) {
            ENJIN_LOG_FATAL(Player, "Failed to open game.enjpak from: %s", pakPath.c_str());
            return;
        }

        // Read build manifest
        auto manifestData = m_AssetReader.ReadFile("_build/manifest.json");
        if (manifestData.empty()) {
            ENJIN_LOG_FATAL(Player, "Build manifest missing from pack");
            return;
        }

        try {
            std::string manifestStr(manifestData.begin(), manifestData.end());
            auto manifest = nlohmann::json::parse(manifestStr);

            m_WindowTitle = manifest.value("windowTitle", "Enjin Game");
            m_WindowWidth = manifest.value("windowWidth", 1280u);
            m_WindowHeight = manifest.value("windowHeight", 720u);
            m_Fullscreen = manifest.value("fullscreen", false);
            m_StartScene = manifest.value("startScene", "");

            ENJIN_LOG_INFO(Player, "Game: %s (%ux%u)", m_WindowTitle.c_str(), m_WindowWidth, m_WindowHeight);
        } catch (const std::exception& e) {
            ENJIN_LOG_ERROR(Player, "Error parsing build manifest: %s", e.what());
            return;
        }

        // Window title is set via WindowDesc at creation time in Application::Run()
        // (no SetTitle method on Window — title comes from WindowDesc.title)

        // Initialize Vulkan renderer
        m_Renderer = std::make_unique<Enjin::Renderer::VulkanRenderer>();
        if (!m_Renderer->Initialize(GetWindow())) {
            ENJIN_LOG_FATAL(Player, "Failed to initialize Vulkan renderer");
            m_Renderer.reset();
            return;
        }

        GetWindow()->SetResizeCallback([this](Enjin::u32, Enjin::u32) {
            if (m_Renderer) {
                m_Renderer->SetFramebufferResized(true);
            }
        });

        // Setup camera
        m_Camera = std::make_unique<Enjin::Renderer::Camera>();
        m_Camera->SetPerspective(45.0f, 16.0f / 9.0f, 0.1f, 1000.0f);
        m_Camera->SetPosition(Enjin::Math::Vector3(0.0f, 2.5f, 7.0f));

        m_CameraController = std::make_unique<Enjin::Renderer::CameraController>(m_Camera.get());

        // Create ECS world
        m_World = std::make_unique<Enjin::ECS::World>();

        // Setup render system
        m_RenderSystem = m_World->RegisterSystem<Enjin::ECS::RenderSystem>(m_World.get(), m_Renderer.get());
        m_RenderSystem->SetCamera(m_Camera.get());
        m_RenderSystem->Initialize();

        // Initialize ImGui layer for pause menu and dialogue overlays
        m_ImGuiLayer = std::make_unique<Enjin::GUI::ImGuiLayer>();
        if (!m_ImGuiLayer->Initialize(GetWindow(), m_Renderer.get())) {
            ENJIN_LOG_WARN(Player, "Failed to initialize ImGui layer — menus will not render");
            m_ImGuiLayer.reset();
        }

        // Connect game menu to input system
        m_GameMenu.SetInputMap(&m_InputMap);
        m_GameMenu.SetGameTitle(m_WindowTitle.empty() ? "Game" : m_WindowTitle);

        // Initialize audio system
        Enjin::Audio::AudioManager::Get().Initialize();

        // Initialize scripting engine
        if (m_ScriptEngine.Init()) {
            Enjin::Scripting::RegisterAllBindings(m_ScriptEngine.GetASEngine());
            m_ScriptEngine.SetWorld(m_World.get());
            m_ScriptEngine.SetScriptDirectory("scripts");

            m_ScriptSystem.SetWorld(m_World.get());
            m_ScriptSystem.SetScriptEngine(&m_ScriptEngine);
            m_ScriptSystem.SetCoroutineScheduler(&m_CoroutineScheduler);

            m_CoroutineScheduler.SetEngine(m_ScriptEngine.GetASEngine());
            m_ScriptEventBus.SetScriptEngine(&m_ScriptEngine);

            ENJIN_LOG_INFO(Player, "Script engine initialized");
        } else {
            ENJIN_LOG_WARN(Player, "Failed to initialize script engine");
        }

        // Show splash screen before loading game
        SetupSplashScreen();

        m_Initialized = true;
        ENJIN_LOG_INFO(Player, "Player initialized");
    }

    void Shutdown() override {
        ENJIN_LOG_INFO(Player, "Player shutting down...");

        // Shutdown scripts before world is destroyed
        m_ScriptSystem.ShutdownAllScripts();
        m_ScriptSystem.SetEnabled(false);
        m_CoroutineScheduler.Clear();
        m_ScriptEventBus.Clear();
        m_ScriptEngine.Shutdown();

        Enjin::Audio::AudioManager::Get().Shutdown();

        if (m_ImGuiLayer) {
            m_ImGuiLayer->Shutdown();
            m_ImGuiLayer.reset();
        }

        if (m_RenderSystem) {
            m_RenderSystem->Shutdown();
            m_RenderSystem = nullptr;
        }
        m_World.reset();
        m_CameraController.reset();
        m_Camera.reset();
        m_Renderer.reset();
        m_AssetReader.Close();
    }

    void Update(Enjin::f32 deltaTime) override {
        if (!m_Initialized) return;

        // Splash screen phase
        if (m_ShowingSplash) {
            m_SplashTimer -= deltaTime;

            // Fade in during first second
            if (m_SplashTimer > m_SplashDuration - 1.0f) {
                m_SplashAlpha = 1.0f - (m_SplashTimer - (m_SplashDuration - 1.0f));
            }
            // Fade out during last 0.5 seconds
            else if (m_SplashTimer < 0.5f) {
                m_SplashAlpha = m_SplashTimer / 0.5f;
            } else {
                m_SplashAlpha = 1.0f;
            }

            // Update splash text opacity
            UpdateSplashAlpha();

            // Skip on any key/click
            if (m_SplashTimer < m_SplashDuration - 0.3f) {
                if (Enjin::Input::IsKeyPressed(Enjin::KeyCode::Escape) ||
                    Enjin::Input::IsKeyPressed(Enjin::KeyCode::Space) ||
                    Enjin::Input::IsKeyPressed(Enjin::KeyCode::Enter) ||
                    Enjin::Input::IsMouseButtonPressed(Enjin::MouseButton::Left)) {
                    m_SplashTimer = 0.0f;
                }
            }

            if (m_SplashTimer <= 0.0f) {
                EndSplashScreen();
            }
            return;
        }

        // Update audio
        Enjin::Audio::AudioManager::Get().Update();

        // Update input
        m_InputMap.Update(deltaTime);

        // Update camera
        if (m_CameraController) {
            m_CameraController->Update(deltaTime);
        }

        // ESC to toggle pause menu
        if (Enjin::Input::IsKeyPressed(Enjin::KeyCode::Escape)) {
            if (m_GameMenu.IsMenuOpen()) {
                m_GameMenu.HideAll();
            } else {
                m_GameMenu.ShowScreen(Enjin::GUI::MenuScreen::PauseMenu);
            }
        }

        // Update active dialogue typewriter
        if (!m_GameMenu.IsMenuOpen()) {
            UpdateDialogue(deltaTime);
        }

        // Update scripts
        if (!m_GameMenu.IsMenuOpen()) {
            m_ScriptSystem.Update(deltaTime);
            m_CoroutineScheduler.EndOfFrame();
        }
    }

    void Render() override {
        if (!m_Initialized || !m_Renderer) return;

        if (!m_Renderer->BeginFrame()) return;

        // Update camera aspect ratio
        auto extent = m_Renderer->GetSwapchainExtent();
        if (extent.width > 0 && extent.height > 0 && m_Camera) {
            Enjin::f32 aspect = static_cast<Enjin::f32>(extent.width) / static_cast<Enjin::f32>(extent.height);
            m_Camera->SetPerspective(45.0f, aspect, 0.1f, 1000.0f);
        }

        // Detect splitscreen: multiple active cameras with non-default viewports
        if (m_World && m_RenderSystem) {
            auto allCameras = Enjin::ECS::CameraManager::GetAllActiveCameras(m_World.get());
            bool useSplitscreen = false;

            if (allCameras.size() > 1) {
                for (auto camEntity : allCameras) {
                    auto* cc = m_World->GetComponent<Enjin::ECS::CameraComponent>(camEntity);
                    if (cc && (cc->viewportX != 0.0f || cc->viewportY != 0.0f ||
                               cc->viewportWidth != 1.0f || cc->viewportHeight != 1.0f)) {
                        useSplitscreen = true;
                        break;
                    }
                }
            }

            if (useSplitscreen) {
                std::vector<Enjin::ECS::ViewportCamera> viewports;
                for (auto camEntity : allCameras) {
                    auto* cc = m_World->GetComponent<Enjin::ECS::CameraComponent>(camEntity);
                    if (!cc) continue;
                    Enjin::ECS::ViewportCamera vc;
                    vc.entity = camEntity;
                    vc.viewportX = cc->viewportX;
                    vc.viewportY = cc->viewportY;
                    vc.viewportWidth = cc->viewportWidth;
                    vc.viewportHeight = cc->viewportHeight;
                    viewports.push_back(vc);
                    if (viewports.size() >= Enjin::ECS::RenderSystem::MAX_SPLITSCREEN_VIEWPORTS) break;
                }
                m_RenderSystem->SetMainPassSplitscreen(viewports);
            } else {
                m_RenderSystem->SetMainPassSplitscreen({});
            }
        }

        if (m_World) {
            m_World->Update(0.0f);
        }

        // Render ImGui overlays (pause menu, dialogue)
        VkCommandBuffer cmd = m_Renderer->GetCurrentCommandBuffer();
        if (m_ImGuiLayer && cmd != VK_NULL_HANDLE) {
            m_ImGuiLayer->BeginFrame();

            // Pause menu
            if (m_GameMenu.IsMenuOpen()) {
                m_GameMenu.Render(static_cast<Enjin::f32>(extent.width),
                                  static_cast<Enjin::f32>(extent.height));
            }

            // Runtime dialogue overlay
            if (!m_ShowingSplash) {
                DrawDialogueOverlay();
            }

            m_ImGuiLayer->EndFrame(cmd);
        }

        m_Renderer->EndFrame();
    }

private:
    void SetupSplashScreen() {
        if (!m_World || !m_RenderSystem) return;

        m_ShowingSplash = true;
        m_SplashTimer = m_SplashDuration;
        m_SplashAlpha = 0.0f;

        // Dark background via skybox
        Enjin::Renderer::SkyboxConfig splashSky;
        splashSky.type = Enjin::Renderer::SkyboxType::SolidColor;
        splashSky.solidColor = Enjin::Math::Vector3(0.04f, 0.04f, 0.06f);
        m_RenderSystem->SetSkybox(splashSky);

        // Camera looking at origin
        if (m_Camera) {
            m_Camera->SetPosition(Enjin::Math::Vector3(0.0f, 0.0f, 5.0f));
            m_Camera->SetLookAt(
                Enjin::Math::Vector3(0.0f, 0.0f, 5.0f),
                Enjin::Math::Vector3(0.0f, 0.0f, 0.0f),
                Enjin::Math::Vector3(0.0f, 1.0f, 0.0f));
        }

        // Title: "TEGE"
        m_SplashTitleEntity = m_World->CreateEntity();
        m_World->AddComponent<Enjin::ECS::NameComponent>(m_SplashTitleEntity, "SplashTitle");
        auto& titleTransform = m_World->AddComponent<Enjin::ECS::TransformComponent>(m_SplashTitleEntity);
        titleTransform.position = Enjin::Math::Vector3(0.0f, 0.3f, 0.0f);
        auto& titleText = m_World->AddComponent<Enjin::ECS::TextComponent>(m_SplashTitleEntity);
        titleText.text = "TEGE";
        titleText.fontSize = 72.0f;
        titleText.textColor = Enjin::Math::Vector3(0.85f, 0.88f, 1.0f);

        // Subtitle: "by marty64"
        m_SplashSubEntity = m_World->CreateEntity();
        m_World->AddComponent<Enjin::ECS::NameComponent>(m_SplashSubEntity, "SplashSub");
        auto& subTransform = m_World->AddComponent<Enjin::ECS::TransformComponent>(m_SplashSubEntity);
        subTransform.position = Enjin::Math::Vector3(0.0f, -0.5f, 0.0f);
        auto& subText = m_World->AddComponent<Enjin::ECS::TextComponent>(m_SplashSubEntity);
        subText.text = "by marty64";
        subText.fontSize = 28.0f;
        subText.textColor = Enjin::Math::Vector3(0.5f, 0.52f, 0.6f);

        // Light so text is visible
        m_SplashLightEntity = m_World->CreateEntity();
        m_World->AddComponent<Enjin::ECS::NameComponent>(m_SplashLightEntity, "SplashLight");
        auto& lightTransform = m_World->AddComponent<Enjin::ECS::TransformComponent>(m_SplashLightEntity);
        lightTransform.rotation = Enjin::Math::Quaternion(
            Enjin::Math::Vector3(1, 0, 0), Enjin::Math::Radians(-30.0f));
        auto& light = m_World->AddComponent<Enjin::ECS::LightComponent>(m_SplashLightEntity);
        light.type = Enjin::ECS::LightType::Directional;
        light.color = Enjin::Math::Vector3(1.0f, 1.0f, 1.0f);
        light.intensity = 2.0f;

        ENJIN_LOG_INFO(Player, "Splash screen displayed");
    }

    void UpdateSplashAlpha() {
        if (!m_World) return;
        auto* titleText = m_World->GetComponent<Enjin::ECS::TextComponent>(m_SplashTitleEntity);
        auto* subText = m_World->GetComponent<Enjin::ECS::TextComponent>(m_SplashSubEntity);
        if (titleText) {
            titleText->textColor = Enjin::Math::Vector3(0.85f * m_SplashAlpha, 0.88f * m_SplashAlpha, 1.0f * m_SplashAlpha);
        }
        if (subText) {
            subText->textColor = Enjin::Math::Vector3(0.5f * m_SplashAlpha, 0.52f * m_SplashAlpha, 0.6f * m_SplashAlpha);
        }
    }

    void EndSplashScreen() {
        m_ShowingSplash = false;

        // Remove splash entities
        if (m_World) {
            m_World->DestroyEntity(m_SplashTitleEntity);
            m_World->DestroyEntity(m_SplashSubEntity);
            m_World->DestroyEntity(m_SplashLightEntity);
        }
        m_SplashTitleEntity = 0;
        m_SplashSubEntity = 0;
        m_SplashLightEntity = 0;

        // Load the actual game scene
        if (!m_StartScene.empty()) {
            LoadSceneFromPack(m_StartScene);
        }

        // Start scripts on loaded entities
        if (m_ScriptEngine.GetASEngine()) {
            m_ScriptSystem.SetEnabled(true);
            Enjin::Scripting::SetBindingsWorld(m_World.get());
            m_ScriptSystem.InitializeAllScripts();
        }

        ENJIN_LOG_INFO(Player, "Splash screen ended, game loaded");
    }

    bool LoadSceneFromPack(const std::string& scenePath) {
        auto sceneData = m_AssetReader.ReadFile(scenePath);
        if (sceneData.empty()) {
            ENJIN_LOG_ERROR(Player, "Failed to read scene from pack: %s", scenePath.c_str());
            return false;
        }

        std::string sceneStr(sceneData.begin(), sceneData.end());
        Enjin::Scene::SceneSerializer serializer(m_World.get());
        auto result = serializer.LoadFromString(sceneStr, true);

        if (!result.success) {
            ENJIN_LOG_ERROR(Player, "Failed to load scene: %s (%s)", scenePath.c_str(), result.error.c_str());
            return false;
        }

        // Apply skybox
        if (m_RenderSystem) {
            m_RenderSystem->SetSkybox(serializer.GetSkyboxConfig());
        }

        ENJIN_LOG_INFO(Player, "Loaded scene: %s (%zu entities)", scenePath.c_str(), result.entities.size());
        return true;
    }

    // Default pack key — matches the build pipeline default
    static constexpr const char* PACK_KEY = "enjin_default_pack_key_2025";

    bool m_Initialized = false;

    // Splash screen
    bool m_ShowingSplash = false;
    Enjin::f32 m_SplashTimer = 0.0f;
    Enjin::f32 m_SplashAlpha = 0.0f;
    static constexpr Enjin::f32 m_SplashDuration = 3.0f;
    Enjin::ECS::Entity m_SplashTitleEntity = 0;
    Enjin::ECS::Entity m_SplashSubEntity = 0;
    Enjin::ECS::Entity m_SplashLightEntity = 0;

    // Build manifest data
    std::string m_WindowTitle;
    Enjin::u32 m_WindowWidth = 1280;
    Enjin::u32 m_WindowHeight = 720;
    bool m_Fullscreen = false;
    std::string m_StartScene;

    // Core systems
    std::unique_ptr<Enjin::Renderer::VulkanRenderer> m_Renderer;
    std::unique_ptr<Enjin::Renderer::Camera> m_Camera;
    std::unique_ptr<Enjin::Renderer::CameraController> m_CameraController;
    std::unique_ptr<Enjin::ECS::World> m_World;
    Enjin::ECS::RenderSystem* m_RenderSystem = nullptr;

    // Asset reader
    Enjin::Build::AssetReader m_AssetReader;

    // Input & menus
    Enjin::InputSystem::InputActionMap m_InputMap;
    Enjin::GUI::GameMenuSystem m_GameMenu;

    // Scripting
    Enjin::Scripting::ScriptEngine m_ScriptEngine;
    Enjin::Scripting::ScriptSystem m_ScriptSystem;
    Enjin::Scripting::CoroutineScheduler m_CoroutineScheduler;
    Enjin::Scripting::ScriptEventBus m_ScriptEventBus;

    // ImGui overlay
    std::unique_ptr<Enjin::GUI::ImGuiLayer> m_ImGuiLayer;

    // Dialogue system
    Enjin::ECS::Entity m_ActiveDialogueEntity = 0;

    void UpdateDialogue(Enjin::f32 deltaTime) {
        if (!m_World) return;

        Enjin::ECS::Entity activeDialogue = 0;

        for (Enjin::ECS::Entity entity : m_World->GetAllEntities()) {
            if (!m_World->HasComponent<Enjin::ECS::DialogueComponent>(entity)) continue;
            auto* dlg = m_World->GetComponent<Enjin::ECS::DialogueComponent>(entity);
            if (!dlg || dlg->dialogueLines.empty() || dlg->IsComplete()) continue;

            activeDialogue = entity;
            auto& d = *dlg;

            // Advance typewriter
            if (d.isTyping && d.currentLine < d.dialogueLines.size()) {
                d.charTimer += deltaTime;
                while (d.charTimer >= d.charDelay && d.currentChar < d.dialogueLines[d.currentLine].size()) {
                    d.currentChar++;
                    d.charTimer -= d.charDelay;
                }
                if (d.currentChar >= d.dialogueLines[d.currentLine].size()) {
                    d.isTyping = false;
                    d.waitingForInput = true;
                }
            }

            // Input: advance
            if (d.waitingForInput) {
                if (Enjin::Input::IsKeyPressed(Enjin::KeyCode::Space) ||
                    Enjin::Input::IsKeyPressed(Enjin::KeyCode::Enter) ||
                    Enjin::Input::IsMouseButtonPressed(Enjin::MouseButton::Left)) {
                    if (d.currentLine + 1 >= d.dialogueLines.size() && !d.choices.empty()) {
                        // Wait for choice selection
                    } else {
                        d.currentLine++;
                        d.currentChar = 0;
                        d.charTimer = 0.0f;
                        d.waitingForInput = false;
                        if (!d.IsComplete()) d.isTyping = true;
                    }
                }
            }

            // Skip to end of line while typing
            if (d.isTyping) {
                if (Enjin::Input::IsKeyPressed(Enjin::KeyCode::Space) ||
                    Enjin::Input::IsKeyPressed(Enjin::KeyCode::Enter)) {
                    d.currentChar = static_cast<Enjin::u32>(d.dialogueLines[d.currentLine].size());
                    d.isTyping = false;
                    d.waitingForInput = true;
                }
            }

            // Choice navigation
            if (d.waitingForInput && !d.choices.empty() &&
                d.currentLine + 1 >= d.dialogueLines.size()) {
                if (Enjin::Input::IsKeyPressed(Enjin::KeyCode::Up) ||
                    Enjin::Input::IsKeyPressed(Enjin::KeyCode::W)) {
                    d.selectedChoice--;
                    if (d.selectedChoice < 0)
                        d.selectedChoice = static_cast<Enjin::i32>(d.choices.size()) - 1;
                }
                if (Enjin::Input::IsKeyPressed(Enjin::KeyCode::Down) ||
                    Enjin::Input::IsKeyPressed(Enjin::KeyCode::S)) {
                    d.selectedChoice++;
                    if (d.selectedChoice >= static_cast<Enjin::i32>(d.choices.size()))
                        d.selectedChoice = 0;
                }
                if (Enjin::Input::IsKeyPressed(Enjin::KeyCode::Enter) ||
                    Enjin::Input::IsKeyPressed(Enjin::KeyCode::Space)) {
                    d.currentLine = static_cast<Enjin::u32>(d.dialogueLines.size());
                    d.waitingForInput = false;
                }
            }

            break;
        }

        m_ActiveDialogueEntity = activeDialogue;
    }

    void DrawDialogueOverlay() {
        if (!m_World || m_ActiveDialogueEntity == 0) return;

        auto* dlg = m_World->GetComponent<Enjin::ECS::DialogueComponent>(m_ActiveDialogueEntity);
        if (!dlg || dlg->IsComplete()) return;

        ImGuiIO& io = ImGui::GetIO();
        Enjin::f32 screenW = io.DisplaySize.x;
        Enjin::f32 screenH = io.DisplaySize.y;

        Enjin::f32 boxW = screenW * 0.75f;
        Enjin::f32 boxH = 140.0f;
        Enjin::f32 boxX = (screenW - boxW) * 0.5f;
        Enjin::f32 boxY = screenH - boxH - 30.0f;
        Enjin::f32 padding = 16.0f;

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(padding, padding));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.0f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.05f, 0.1f, 0.92f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.4f, 0.45f, 0.65f, 0.8f));

        ImGui::SetNextWindowPos(ImVec2(boxX, boxY));
        ImGui::SetNextWindowSize(ImVec2(boxW, boxH));

        if (ImGui::Begin("##DialogueBox", nullptr, flags)) {
            if (!dlg->speakerName.empty()) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.75f, 1.0f, 1.0f));
                ImGui::Text("%s", dlg->speakerName.c_str());
                ImGui::PopStyleColor();
                ImGui::Separator();
                ImGui::Spacing();
            }

            std::string visibleText = dlg->GetVisibleText();
            if (!visibleText.empty()) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.95f, 0.98f, 1.0f));
                ImGui::TextWrapped("%s", visibleText.c_str());
                ImGui::PopStyleColor();
            }

            if (dlg->isTyping) {
                ImGui::SameLine(0, 0);
                Enjin::f32 blink = std::fmod(static_cast<Enjin::f32>(ImGui::GetTime()) * 3.0f, 2.0f);
                if (blink < 1.0f) {
                    ImGui::TextColored(ImVec4(0.7f, 0.8f, 1.0f, 0.8f), "_");
                }
            }

            if (dlg->waitingForInput && dlg->choices.empty()) {
                Enjin::f32 bounce = std::sin(static_cast<Enjin::f32>(ImGui::GetTime()) * 4.0f) * 0.3f + 0.7f;
                ImGui::SetCursorPosY(boxH - padding - ImGui::GetTextLineHeight());
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.6f, 0.8f, bounce));
                ImGui::Text("[Space]");
                ImGui::PopStyleColor();
            }
        }
        ImGui::End();

        // Choice box
        bool showChoices = dlg->waitingForInput && !dlg->choices.empty() &&
                           dlg->currentLine + 1 >= dlg->dialogueLines.size();
        if (showChoices) {
            Enjin::f32 choiceH = static_cast<Enjin::f32>(dlg->choices.size()) * 28.0f + padding * 2.0f;
            Enjin::f32 choiceW = 300.0f;
            Enjin::f32 choiceX = boxX + boxW - choiceW - 10.0f;
            Enjin::f32 choiceY = boxY - choiceH - 8.0f;

            ImGui::SetNextWindowPos(ImVec2(choiceX, choiceY));
            ImGui::SetNextWindowSize(ImVec2(choiceW, choiceH));

            if (ImGui::Begin("##ChoiceBox", nullptr, flags)) {
                for (Enjin::usize i = 0; i < dlg->choices.size(); ++i) {
                    bool selected = (static_cast<Enjin::i32>(i) == dlg->selectedChoice);
                    if (selected) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.6f, 1.0f));
                        ImGui::Text("> %s", dlg->choices[i].text.c_str());
                        ImGui::PopStyleColor();
                    } else {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.75f, 1.0f));
                        ImGui::Text("  %s", dlg->choices[i].text.c_str());
                        ImGui::PopStyleColor();
                    }
                }
            }
            ImGui::End();
        }

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(3);
    }
};

Enjin::Application* CreateApplication() {
    return new GamePlayer();
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    // Set working directory to exe location so relative paths work
    Enjin::Platform::SetWorkingDirectoryToExecutableDirectory();

    Enjin::Application* app = CreateApplication();
    int result = app->Run();
    delete app;
    return result;
}
