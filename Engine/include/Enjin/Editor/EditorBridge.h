#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Editor/EditorProtocol.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Quaternion.h"
#include <string>
#include <memory>

namespace Enjin {

namespace ECS { class World; }
namespace Scene { class SceneSerializer; class SceneManager; }

namespace Editor {

// Editor-runtime bridge — manages cross-process communication and state sync.
//
// Architecture (Glacier-inspired):
//   Editor process: owns ImGui UI, scene editing state, undo/redo history.
//   Runtime process: owns Vulkan renderer, ECS World, physics, game systems.
//   On runtime crash: editor restarts runtime, resends scene state. Zero data loss.
//
// The bridge can operate in two modes:
//   1. In-process (default): editor and runtime share memory (current architecture)
//   2. Out-of-process: editor spawns runtime as child process, communicates via IPC
//
// Mode 2 is the target for crash isolation. Mode 1 remains for backwards compatibility
// and for platforms where process isolation isn't practical (Web/Emscripten).
class ENJIN_API EditorBridge {
public:
    EditorBridge();
    ~EditorBridge();

    // Initialize in-process mode (shared World pointer, no IPC)
    bool InitializeInProcess(ECS::World* world, Scene::SceneManager* sceneManager);

    // Initialize out-of-process mode (spawn runtime child process)
    bool InitializeOutOfProcess(const std::string& runtimeExecutablePath,
                                 const std::string& transportEndpoint = "ipc:///tmp/enjin_editor");

    void Shutdown();

    // Per-frame update: process incoming messages, send pending changes
    void Update(f32 deltaTime);

    // --- Editor → Runtime commands ---

    // Scene manipulation
    void CreateEntity(const std::string& name);
    void DestroyEntity(ECS::Entity entity);
    void ModifyComponent(ECS::Entity entity, const std::string& componentType,
                          const std::string& propertyKey, const std::string& valueJson);

    // Play mode
    void Play();
    void Pause();
    void Stop();
    void StepFrame();

    // Viewport
    void SyncCamera(const Math::Vector3& position, const Math::Quaternion& rotation, f32 fov);
    void ResizeViewport(u32 width, u32 height);

    // Asset management
    void ReloadAsset(const std::string& assetPath);
    void LoadScene(const std::string& scenePath);

    // --- State queries ---

    bool IsConnected() const;
    bool IsOutOfProcess() const { return m_OutOfProcess; }
    bool IsRuntimeAlive() const;

    // Scene state shadow copy (for crash recovery)
    const std::string& GetSceneStateShadow() const { return m_SceneStateShadow; }
    void UpdateSceneStateShadow(); // Serialize current scene to shadow copy

    // --- Runtime crash recovery ---

    // Called when runtime process exits unexpectedly.
    // Restarts runtime and resends scene state from shadow copy.
    bool RecoverFromCrash();

private:
    void SendBridgeMessage(EditorMessageType type, const std::string& payload = "");
    void HandleIncomingMessages();

    // Process management (out-of-process mode)
    bool SpawnRuntimeProcess(const std::string& executablePath);
    void KillRuntimeProcess();

    bool m_Initialized = false;
    bool m_OutOfProcess = false;
    u64 m_NextSequenceId = 1;

    // In-process mode: direct pointers
    ECS::World* m_World = nullptr;
    Scene::SceneManager* m_SceneManager = nullptr;

    // Out-of-process mode: IPC transport
    std::unique_ptr<IEditorTransport> m_Transport;
    EditorMessageDispatcher m_Dispatcher;
    std::string m_RuntimeExecutablePath;

    // Scene state shadow copy (JSON string — updated periodically for crash recovery)
    std::string m_SceneStateShadow;
    f32 m_ShadowUpdateTimer = 0.0f;
    f32 m_ShadowUpdateInterval = 5.0f; // Seconds between shadow updates

    // Runtime process handle (platform-specific)
#ifdef _WIN32
    void* m_ProcessHandle = nullptr;  // HANDLE
#else
    i32 m_ProcessPid = -1;
#endif

    // Heartbeat
    f32 m_HeartbeatTimer = 0.0f;
    f32 m_LastPongTime = 0.0f;
    static constexpr f32 HEARTBEAT_INTERVAL = 2.0f;
    static constexpr f32 HEARTBEAT_TIMEOUT = 10.0f;
};

} // namespace Editor
} // namespace Enjin
