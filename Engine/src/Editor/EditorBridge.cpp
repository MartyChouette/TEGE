#include "Enjin/Editor/EditorBridge.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/Scene/SceneSerializer.h"
#include "Enjin/Scene/SceneManager.h"
#include "Enjin/Logging/Log.h"

namespace Enjin {
namespace Editor {

EditorBridge::EditorBridge() = default;

EditorBridge::~EditorBridge() {
    Shutdown();
}

bool EditorBridge::InitializeInProcess(ECS::World* world, Scene::SceneManager* sceneManager) {
    m_World = world;
    m_SceneManager = sceneManager;
    m_OutOfProcess = false;
    m_Initialized = true;
    ENJIN_LOG_INFO(Editor, "EditorBridge initialized (in-process mode)");
    return true;
}

bool EditorBridge::InitializeOutOfProcess(const std::string& runtimeExecutablePath,
                                            const std::string& transportEndpoint) {
    m_RuntimeExecutablePath = runtimeExecutablePath;
    m_OutOfProcess = true;

    // TODO: Create transport (shared memory on desktop, WebSocket for web)
    // TODO: Spawn runtime child process
    // For now, log the intent
    ENJIN_LOG_INFO(Editor, "EditorBridge out-of-process mode: runtime='%s', endpoint='%s'",
                   runtimeExecutablePath.c_str(), transportEndpoint.c_str());

    m_Initialized = true;
    return true;
}

void EditorBridge::Shutdown() {
    if (!m_Initialized) return;

    if (m_OutOfProcess) {
        KillRuntimeProcess();
    }

    m_Transport.reset();
    m_World = nullptr;
    m_SceneManager = nullptr;
    m_Initialized = false;
}

void EditorBridge::Update(f32 deltaTime) {
    if (!m_Initialized) return;

    if (m_OutOfProcess && m_Transport) {
        // Process incoming messages from runtime
        m_Dispatcher.DispatchAll(*m_Transport);

        // Heartbeat
        m_HeartbeatTimer += deltaTime;
        if (m_HeartbeatTimer >= HEARTBEAT_INTERVAL) {
            m_HeartbeatTimer = 0.0f;
            SendMessage(EditorMessageType::Ping);
        }

        // Check for runtime death
        if (m_LastPongTime > 0.0f && (deltaTime - m_LastPongTime) > HEARTBEAT_TIMEOUT) {
            ENJIN_LOG_ERROR(Editor, "Runtime heartbeat timeout — attempting recovery");
            RecoverFromCrash();
        }
    }

    // Periodically update scene state shadow copy (for crash recovery)
    m_ShadowUpdateTimer += deltaTime;
    if (m_ShadowUpdateTimer >= m_ShadowUpdateInterval) {
        m_ShadowUpdateTimer = 0.0f;
        UpdateSceneStateShadow();
    }
}

// --- Editor → Runtime commands ---

void EditorBridge::CreateEntity(const std::string& name) {
    if (m_OutOfProcess) {
        SendMessage(EditorMessageType::EntityCreate, name);
    } else if (m_World) {
        ECS::Entity e = m_World->CreateEntity();
        if (!name.empty()) {
            m_World->AddComponent<ECS::NameComponent>(e, ECS::NameComponent{name});
        }
    }
}

void EditorBridge::DestroyEntity(ECS::Entity entity) {
    if (m_OutOfProcess) {
        SendMessage(EditorMessageType::EntityDestroy, std::to_string(entity));
    } else if (m_World) {
        m_World->DestroyEntity(entity);
    }
}

void EditorBridge::ModifyComponent(ECS::Entity entity, const std::string& componentType,
                                    const std::string& propertyKey, const std::string& valueJson) {
    if (m_OutOfProcess) {
        // Pack as JSON: {"entity": N, "component": "...", "key": "...", "value": ...}
        std::string payload = "{\"entity\":" + std::to_string(entity) +
            ",\"component\":\"" + componentType +
            "\",\"key\":\"" + propertyKey +
            "\",\"value\":" + valueJson + "}";
        SendMessage(EditorMessageType::ComponentModify, payload);
    }
    // In-process mode: caller modifies components directly
}

void EditorBridge::Play() { SendMessage(EditorMessageType::PlayModePlay); }
void EditorBridge::Pause() { SendMessage(EditorMessageType::PlayModePause); }
void EditorBridge::Stop() { SendMessage(EditorMessageType::PlayModeStop); }
void EditorBridge::StepFrame() { SendMessage(EditorMessageType::PlayModeStep); }

void EditorBridge::SyncCamera(const Math::Vector3& position, const Math::Quaternion& rotation, f32 fov) {
    if (!m_OutOfProcess) return;
    std::string payload = "{\"pos\":[" + std::to_string(position.x) + "," +
        std::to_string(position.y) + "," + std::to_string(position.z) + "],\"fov\":" +
        std::to_string(fov) + "}";
    SendMessage(EditorMessageType::ViewportCameraSync, payload);
}

void EditorBridge::ResizeViewport(u32 width, u32 height) {
    if (!m_OutOfProcess) return;
    SendMessage(EditorMessageType::ViewportResize,
                "{\"w\":" + std::to_string(width) + ",\"h\":" + std::to_string(height) + "}");
}

void EditorBridge::ReloadAsset(const std::string& assetPath) {
    SendMessage(EditorMessageType::AssetReload, assetPath);
}

void EditorBridge::LoadScene(const std::string& scenePath) {
    if (m_OutOfProcess) {
        SendMessage(EditorMessageType::SceneLoad, scenePath);
    } else if (m_SceneManager) {
        m_SceneManager->LoadScene(scenePath);
    }
}

// --- State queries ---

bool EditorBridge::IsConnected() const {
    if (!m_OutOfProcess) return m_Initialized;
    return m_Transport && m_Transport->IsConnected();
}

bool EditorBridge::IsRuntimeAlive() const {
    if (!m_OutOfProcess) return m_Initialized;
#ifdef _WIN32
    // TODO: Check process handle validity
    return m_ProcessHandle != nullptr;
#else
    return m_ProcessPid > 0;
#endif
}

void EditorBridge::UpdateSceneStateShadow() {
    if (!m_World) return;

    Scene::SceneSerializer serializer(m_World);
    m_SceneStateShadow = serializer.SaveToString();
}

bool EditorBridge::RecoverFromCrash() {
    if (!m_OutOfProcess) return false;

    ENJIN_LOG_WARN(Editor, "Recovering from runtime crash...");

    KillRuntimeProcess();

    if (!SpawnRuntimeProcess(m_RuntimeExecutablePath)) {
        ENJIN_LOG_ERROR(Editor, "Failed to restart runtime process");
        return false;
    }

    // Resend scene state from shadow copy
    if (!m_SceneStateShadow.empty()) {
        SendMessage(EditorMessageType::SceneLoad, m_SceneStateShadow);
        ENJIN_LOG_INFO(Editor, "Resent scene state (%zu bytes) to restarted runtime",
                       m_SceneStateShadow.size());
    }

    return true;
}

// --- Private ---

void EditorBridge::SendMessage(EditorMessageType type, const std::string& payload) {
    if (m_OutOfProcess && m_Transport) {
        auto msg = EditorMessage::MakeString(type, payload, m_NextSequenceId++);
        m_Transport->Send(msg);
    }
}

bool EditorBridge::SpawnRuntimeProcess(const std::string& executablePath) {
    // TODO: Platform-specific process creation
    // Windows: CreateProcessA
    // Linux/macOS: fork + execlp
    ENJIN_LOG_INFO(Editor, "Spawning runtime process: %s", executablePath.c_str());
    (void)executablePath;
    return false; // Not yet implemented
}

void EditorBridge::KillRuntimeProcess() {
#ifdef _WIN32
    if (m_ProcessHandle) {
        // TODO: TerminateProcess
        m_ProcessHandle = nullptr;
    }
#else
    if (m_ProcessPid > 0) {
        // TODO: kill(m_ProcessPid, SIGTERM)
        m_ProcessPid = -1;
    }
#endif
}

} // namespace Editor
} // namespace Enjin
