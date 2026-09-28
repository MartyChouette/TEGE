#pragma once
// The voice budget: how many sounds can play at once, and which one gives way
// when a new sound would go over.
//
// Every Play used to allocate its own voice until a flat 256 cap, and at the cap
// the NEW sound was refused whatever it was. A bullet casing could keep a line of
// dialogue from starting. The rules, decided 2026-09-26:
//   - one global budget plus a cap per channel;
//   - when either is full, steal the QUIETEST voice that is not looping, not
//     Music, and not more important than the new one;
//   - if there is no such voice the new sound is refused;
//   - priority comes from the channel, and an AudioSource can override it.
// Priority follows the AudioSource convention: LOWER is MORE important.

#include "Enjin/Platform/Types.h"
#include <vector>

namespace Enjin::Audio {

// Sounds playing at once, across every channel
constexpr u32 kDefaultVoiceBudget = 64;
// Per channel, in AudioChannel order: SFX, Music, UI, Voice
constexpr u32 kDefaultChannelVoiceCaps[4] = {48, 4, 16, 8};
// Per channel priority, lower = more important: SFX, Music, UI, Voice
constexpr i32 kDefaultChannelPriority[4] = {128, 0, 64, 32};
// AudioSourceComponent::voicePriority value meaning "use the channel's"
constexpr i32 kUseChannelPriority = -1;
// AudioChannel::Music, never stolen
constexpr u8 kMusicChannelIndex = 1;

inline i32 ResolveVoicePriority(i32 sourcePriority, u8 channel) {
    if (sourcePriority >= 0) return sourcePriority;
    return channel < 4 ? kDefaultChannelPriority[channel] : kDefaultChannelPriority[0];
}

// One playing voice, as the steal decision sees it
struct VoiceCandidate {
    u8 channel = 0;
    i32 priority = 128;
    bool loop = false;
    f32 audibleVolume = 0.0f;   // after bus volumes and distance
};

// Index into `voices` of the voice to steal for a new sound, or -1 to refuse it.
// `sameChannelOnly` is for a channel cap: only that channel's voices can give way.
inline i32 ChooseVoiceToSteal(const std::vector<VoiceCandidate>& voices, i32 newPriority,
                              u8 newChannel, bool sameChannelOnly) {
    i32 best = -1;
    for (usize i = 0; i < voices.size(); ++i) {
        const VoiceCandidate& v = voices[i];
        if (v.loop || v.channel == kMusicChannelIndex) continue;
        if (sameChannelOnly && v.channel != newChannel) continue;
        if (v.priority < newPriority) continue;   // more important than the new sound
        if (best < 0 || v.audibleVolume < voices[static_cast<usize>(best)].audibleVolume)
            best = static_cast<i32>(i);
    }
    return best;
}

} // namespace Enjin::Audio
