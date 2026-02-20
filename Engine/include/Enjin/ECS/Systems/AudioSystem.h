#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Audio/AudioSystem.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/Logging/Log.h"
#include <unordered_map>

namespace Enjin {
namespace ECS {

// ECS system that drives audio playback from AudioSourceComponent / AudioListenerComponent
class ENJIN_API AudioSystem {
public:
    explicit AudioSystem(World* world) : m_World(world) {}
    ~AudioSystem() { Shutdown(); }

    void Initialize() {
        ENJIN_LOG_INFO(Audio, "AudioSystem initialized");
    }

    void Shutdown() {
        // Stop all tracked sounds
        auto& mgr = Audio::AudioManager::Get();
        for (auto& [entity, channel] : m_ActiveSounds) {
            mgr.StopChannel(channel);
        }
        m_ActiveSounds.clear();
        m_LoadedSounds.clear();
    }

    void Update(f32 deltaTime) {
        if (!m_World) return;

        auto& mgr = Audio::AudioManager::Get();
        if (!mgr.IsInitialized()) return;

        // --- Update listener from AudioListenerComponent ---
        for (Entity entity : m_World->GetEntitiesWithComponent<AudioListenerComponent>()) {
            if (!m_World->IsValid(entity)) continue;
            if (!m_World->HasComponent<TransformComponent>(entity)) continue;

            auto* listener = m_World->GetComponent<AudioListenerComponent>(entity);
            if (!listener->isActive) continue;

            auto* transform = m_World->GetComponent<TransformComponent>(entity);

            // Derive forward vector from rotation quaternion
            auto& rot = transform->rotation;
            Math::Vector3 forward(
                2.0f * (rot.x * rot.z + rot.w * rot.y),
                2.0f * (rot.y * rot.z - rot.w * rot.x),
                1.0f - 2.0f * (rot.x * rot.x + rot.y * rot.y)
            );
            Math::Vector3 up(
                2.0f * (rot.x * rot.y - rot.w * rot.z),
                1.0f - 2.0f * (rot.x * rot.x + rot.z * rot.z),
                2.0f * (rot.y * rot.z + rot.w * rot.x)
            );

            mgr.SetListenerPosition(transform->position, forward, up);
            break; // Only one active listener
        }

        // --- Update audio sources ---
        for (Entity entity : m_World->GetEntitiesWithComponent<AudioSourceComponent>()) {
            if (!m_World->IsValid(entity)) continue;
            if (!m_World->HasComponent<AudioSourceComponent>(entity)) continue;

            auto* src = m_World->GetComponent<AudioSourceComponent>(entity);

            // Ensure sound is loaded
            if (src->soundHandle == 0 && !src->clipPath.empty()) {
                auto cacheIt = m_LoadedSounds.find(src->clipPath);
                if (cacheIt != m_LoadedSounds.end()) {
                    src->soundHandle = cacheIt->second;
                } else {
                    Audio::SoundSettings settings;
                    settings.is3D = src->is3D;
                    settings.loop = src->loop;
                    settings.volume = src->volume;
                    settings.pitch = src->pitch;
                    settings.minDistance = src->minDistance;
                    settings.maxDistance = src->maxDistance;
                    Audio::SoundHandle handle = mgr.LoadSound(src->clipPath, settings);
                    src->soundHandle = handle;
                    m_LoadedSounds[src->clipPath] = handle;
                }
            }

            // Handle playOnAwake
            if (src->playOnAwake && !src->awakeTriggered && src->soundHandle != 0) {
                src->awakeTriggered = true;

                Math::Vector3 pos(0, 0, 0);
                if (m_World->HasComponent<TransformComponent>(entity)) {
                    pos = m_World->GetComponent<TransformComponent>(entity)->position;
                }

                Audio::ChannelHandle ch = mgr.PlaySoundAt(
                    static_cast<Audio::SoundHandle>(src->soundHandle), pos);

                if (ch != Audio::INVALID_CHANNEL) {
                    m_ActiveSounds[entity] = ch;
                    src->isPlaying = true;
                }
            }

            // Update 3D position for playing sounds
            auto activeIt = m_ActiveSounds.find(entity);
            if (activeIt != m_ActiveSounds.end()) {
                auto* backend = mgr.GetBackend();
                if (backend) {
                    if (backend->IsChannelPlaying(activeIt->second)) {
                        if (src->is3D && m_World->HasComponent<TransformComponent>(entity)) {
                            auto& pos = m_World->GetComponent<TransformComponent>(entity)->position;
                            backend->SetChannelPosition(activeIt->second, pos);
                        }
                    } else {
                        // Sound finished
                        src->isPlaying = false;
                        m_ActiveSounds.erase(activeIt);
                    }
                }
            }
        }
    }

    void OnEntityRemoved(Entity entity) {
        auto it = m_ActiveSounds.find(entity);
        if (it != m_ActiveSounds.end()) {
            Audio::AudioManager::Get().StopChannel(it->second);
            m_ActiveSounds.erase(it);
        }
    }

private:
    World* m_World = nullptr;
    std::unordered_map<Entity, Audio::ChannelHandle> m_ActiveSounds;
    std::unordered_map<std::string, Audio::SoundHandle> m_LoadedSounds;
};

} // namespace ECS
} // namespace Enjin
