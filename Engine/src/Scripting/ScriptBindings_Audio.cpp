#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/Audio/SimpleAudio.h"
#include <angelscript.h>
#include <string>
#include <cassert>

using namespace Enjin;
using namespace Enjin::Math;
using namespace Enjin::ECS;

#define AS_CHECK(expr) \
    do { int _r = (expr); assert(_r >= 0); (void)_r; } while(0)

extern ECS::World* s_BindingsWorld;
extern bool ValidateScriptAssetPath(const std::string& path, const char* funcName);

static Audio::SimpleAudio* s_BindingsAudio = nullptr;

// Channel constants for AngelScript
static const u8 s_ChannelSFX   = 0;
static const u8 s_ChannelMusic = 1;
static const u8 s_ChannelUI    = 2;
static const u8 s_ChannelVoice = 3;

namespace Enjin {
namespace Scripting {
void SetBindingsAudio(Audio::SimpleAudio* audio) { s_BindingsAudio = audio; }
} // namespace Scripting
} // namespace Enjin

// ============================================================================
// Audio functions — wired to SimpleAudio + AudioSourceComponent
// ============================================================================

static void Audio_Play(u64 entityId) {
    if (!s_BindingsWorld || !s_BindingsAudio) return;

    Entity entity = static_cast<Entity>(entityId);
    auto* asc = s_BindingsWorld->GetComponent<AudioSourceComponent>(entity);
    if (!asc) return;

    // Load clip if not already loaded
    Audio::AudioClipHandle clip = s_BindingsAudio->LoadClip(asc->clipPath);
    if (clip == Audio::INVALID_AUDIO_CLIP) {
        asc->isPlaying = false;
        asc->soundHandle = 0;
        return;
    }

    auto ch = static_cast<Audio::AudioChannel>(static_cast<u8>(asc->channel));
    // Music and UI channels force non-diegetic (2D) playback
    bool diegetic3D = asc->is3D &&
        ch != Audio::AudioChannel::Music && ch != Audio::AudioChannel::UI;

    if (diegetic3D) {
        auto* tc = s_BindingsWorld->GetComponent<TransformComponent>(entity);
        Vector3 pos = tc ? tc->position : Vector3();
        asc->soundHandle = s_BindingsAudio->Play3D(clip, pos, asc->volume, asc->minDistance, asc->maxDistance, ch);
    } else {
        asc->soundHandle = s_BindingsAudio->Play(clip, asc->volume, asc->pitch, asc->loop, ch);
    }
    asc->isPlaying = true;
}

static void Audio_PlayAtPosition(const std::string& path, const Vector3& pos) {
    if (!s_BindingsAudio) return;
    if (!ValidateScriptAssetPath(path, "Audio_PlayAtPosition")) return;

    Audio::AudioClipHandle clip = s_BindingsAudio->LoadClip(path);
    if (clip == Audio::INVALID_AUDIO_CLIP) return;

    s_BindingsAudio->PlayOneShot3D(clip, pos);
}

static void Audio_Stop(u64 entityId) {
    if (!s_BindingsWorld || !s_BindingsAudio) return;

    Entity entity = static_cast<Entity>(entityId);
    auto* asc = s_BindingsWorld->GetComponent<AudioSourceComponent>(entity);
    if (!asc || asc->soundHandle == 0) return;

    s_BindingsAudio->Stop(asc->soundHandle);
    asc->isPlaying = false;
    asc->soundHandle = 0;
}

static void Audio_StopAll() {
    if (!s_BindingsAudio) return;
    s_BindingsAudio->StopAll();

    // Also update all AudioSourceComponent states
    if (s_BindingsWorld) {
        auto entities = s_BindingsWorld->GetEntitiesWithComponent<AudioSourceComponent>();
        for (Entity e : entities) {
            auto* asc = s_BindingsWorld->GetComponent<AudioSourceComponent>(e);
            if (asc) {
                asc->isPlaying = false;
                asc->soundHandle = 0;
            }
        }
    }
}

static void Audio_SetVolume(u64 entityId, f32 volume) {
    if (!s_BindingsWorld || !s_BindingsAudio) return;

    Entity entity = static_cast<Entity>(entityId);
    auto* asc = s_BindingsWorld->GetComponent<AudioSourceComponent>(entity);
    if (!asc) return;

    asc->volume = volume;
    if (asc->soundHandle != 0) {
        s_BindingsAudio->SetVolume(asc->soundHandle, volume);
    }
}

static void Audio_SetPitch(u64 entityId, f32 pitch) {
    if (!s_BindingsWorld || !s_BindingsAudio) return;

    Entity entity = static_cast<Entity>(entityId);
    auto* asc = s_BindingsWorld->GetComponent<AudioSourceComponent>(entity);
    if (!asc) return;

    asc->pitch = pitch;
    if (asc->soundHandle != 0) {
        s_BindingsAudio->SetPitch(asc->soundHandle, pitch);
    }
}

static bool Audio_IsPlaying(u64 entityId) {
    if (!s_BindingsWorld || !s_BindingsAudio) return false;

    Entity entity = static_cast<Entity>(entityId);
    auto* asc = s_BindingsWorld->GetComponent<AudioSourceComponent>(entity);
    if (!asc || asc->soundHandle == 0) return false;

    return s_BindingsAudio->IsPlaying(asc->soundHandle);
}

static void Audio_SetMasterVolume(f32 volume) {
    if (!s_BindingsAudio) return;
    s_BindingsAudio->SetMasterVolume(volume);
}

static f32 Audio_GetMasterVolume() {
    if (!s_BindingsAudio) return 1.0f;
    return s_BindingsAudio->GetMasterVolume();
}

// Channel volume: 0=SFX, 1=Music, 2=UI, 3=Voice
static void Audio_SetChannelVolume(u8 channel, f32 volume) {
    if (!s_BindingsAudio || channel >= static_cast<u8>(Audio::AudioChannel::Count)) return;
    s_BindingsAudio->SetChannelVolume(static_cast<Audio::AudioChannel>(channel), volume);
}

static f32 Audio_GetChannelVolume(u8 channel) {
    if (!s_BindingsAudio || channel >= static_cast<u8>(Audio::AudioChannel::Count)) return 1.0f;
    return s_BindingsAudio->GetChannelVolume(static_cast<Audio::AudioChannel>(channel));
}

static void Audio_StopChannel(u8 channel) {
    if (!s_BindingsAudio || channel >= static_cast<u8>(Audio::AudioChannel::Count)) return;
    s_BindingsAudio->StopChannel(static_cast<Audio::AudioChannel>(channel));
}

// ============================================================================
// Registration
// ============================================================================

namespace Enjin {
namespace Scripting {

void RegisterAudioBindings(asIScriptEngine* engine) {
    AS_CHECK(engine->RegisterGlobalFunction(
        "void Audio_Play(uint64)",
        asFUNCTION(Audio_Play), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void Audio_PlayAtPosition(const string &in, const Vector3 &in)",
        asFUNCTION(Audio_PlayAtPosition), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void Audio_Stop(uint64)",
        asFUNCTION(Audio_Stop), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void Audio_StopAll()",
        asFUNCTION(Audio_StopAll), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void Audio_SetVolume(uint64, float)",
        asFUNCTION(Audio_SetVolume), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void Audio_SetPitch(uint64, float)",
        asFUNCTION(Audio_SetPitch), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "bool Audio_IsPlaying(uint64)",
        asFUNCTION(Audio_IsPlaying), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void Audio_SetMasterVolume(float)",
        asFUNCTION(Audio_SetMasterVolume), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "float Audio_GetMasterVolume()",
        asFUNCTION(Audio_GetMasterVolume), asCALL_CDECL));

    // Channel volume (0=SFX, 1=Music, 2=UI, 3=Voice)
    AS_CHECK(engine->RegisterGlobalFunction(
        "void Audio_SetChannelVolume(uint8, float)",
        asFUNCTION(Audio_SetChannelVolume), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "float Audio_GetChannelVolume(uint8)",
        asFUNCTION(Audio_GetChannelVolume), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void Audio_StopChannel(uint8)",
        asFUNCTION(Audio_StopChannel), asCALL_CDECL));

    // Channel constants for convenience
    AS_CHECK(engine->RegisterGlobalProperty("const uint8 AUDIO_CHANNEL_SFX", const_cast<u8*>(&s_ChannelSFX)));
    AS_CHECK(engine->RegisterGlobalProperty("const uint8 AUDIO_CHANNEL_MUSIC", const_cast<u8*>(&s_ChannelMusic)));
    AS_CHECK(engine->RegisterGlobalProperty("const uint8 AUDIO_CHANNEL_UI", const_cast<u8*>(&s_ChannelUI)));
    AS_CHECK(engine->RegisterGlobalProperty("const uint8 AUDIO_CHANNEL_VOICE", const_cast<u8*>(&s_ChannelVoice)));
}

} // namespace Scripting
} // namespace Enjin
