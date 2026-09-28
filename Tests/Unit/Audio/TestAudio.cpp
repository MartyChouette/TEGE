#include "EnjinTest.h"
#include "Enjin/Audio/AudioEngine.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/Audio/AudioBus.h"
#include "Enjin/Input/MIDIInput.h"
#include "Enjin/Editor/AudioEventGraph.h"

#include <cmath>

using namespace Enjin;
using namespace Enjin::Audio;
using namespace Enjin::InputSystem;
using namespace Enjin::Editor;

// ===========================================================================
// AudioChannel Enum
// ===========================================================================

ENJIN_TEST(Channel, Values) {
    ENJIN_EXPECT_EQ((int)AudioChannel::SFX, 0);
    ENJIN_EXPECT_EQ((int)AudioChannel::Music, 1);
    ENJIN_EXPECT_EQ((int)AudioChannel::UI, 2);
    ENJIN_EXPECT_EQ((int)AudioChannel::Voice, 3);
    ENJIN_EXPECT_EQ((int)AudioChannel::Count, 4);
}

ENJIN_TEST(Handles, InvalidConstants) {
    ENJIN_EXPECT_EQ(INVALID_SOUND, 0u);
    ENJIN_EXPECT_EQ(INVALID_AUDIO_CLIP, 0u);
}

// ===========================================================================
// Decibel conversion (AudioBus)
//
// SoundSettings, SoundType, AttenuationMode, AudioListener, ChannelHandle and
// AudioUtils::Calculate3DVolume/CrossfadeVolume were all tested here and are
// all gone: they belonged to an IAudioBackend layer that nothing ever
// instantiated, deleted 2026-09-10. Testing the constants of code that cannot
// run is how a dead layer keeps looking alive in a green suite. The dB pair
// moved into AudioBus, which is what a mixer wants them for.
// ===========================================================================

ENJIN_TEST(AudioUtils, DbToLinearZero) {
    f32 linear = Audio::DbToLinear(0.0f);
    ENJIN_EXPECT_FLOAT_NEAR(linear, 1.0f, 0.01f);
}

ENJIN_TEST(AudioUtils, DbToLinearMinus6) {
    // -6dB ≈ 0.5 linear
    f32 linear = Audio::DbToLinear(-6.0f);
    ENJIN_EXPECT_FLOAT_NEAR(linear, 0.5f, 0.05f);
}

ENJIN_TEST(AudioUtils, LinearToDbOne) {
    f32 db = Audio::LinearToDb(1.0f);
    ENJIN_EXPECT_FLOAT_NEAR(db, 0.0f, 0.01f);
}

ENJIN_TEST(AudioUtils, RoundTrip) {
    f32 original = 0.7f;
    f32 db = Audio::LinearToDb(original);
    f32 back = Audio::DbToLinear(db);
    ENJIN_EXPECT_FLOAT_NEAR(back, original, 0.01f);
}





// ===========================================================================
// MIDI Types
// ===========================================================================

ENJIN_TEST(MIDI, MessageTypeValues) {
    ENJIN_EXPECT_EQ((u8)MIDIMessageType::NoteOff, 0x80);
    ENJIN_EXPECT_EQ((u8)MIDIMessageType::NoteOn, 0x90);
    ENJIN_EXPECT_EQ((u8)MIDIMessageType::ControlChange, 0xB0);
    ENJIN_EXPECT_EQ((u8)MIDIMessageType::PitchBend, 0xE0);
}

ENJIN_TEST(MIDI, EventDefaults) {
    MIDIEvent evt;
    ENJIN_EXPECT_EQ((u8)evt.type, (u8)MIDIMessageType::NoteOff);
    ENJIN_EXPECT_EQ(evt.channel, 0u);
    ENJIN_EXPECT_EQ(evt.data1, 0u);
    ENJIN_EXPECT_EQ(evt.data2, 0u);
}

// ===========================================================================
// Voice budget: which playing sound gives way to a new one
// ===========================================================================

ENJIN_TEST(VoiceBudget, test_voice_budget_full_steals_quietest_eligible) {
    // Arrange: SFX 0.8, SFX 0.2, a quiet looping SFX, quiet Music
    std::vector<VoiceCandidate> voices = {
        {0, 128, false, 0.8f},
        {0, 128, false, 0.2f},
        {0, 128, true, 0.05f},    // looping: never stolen
        {1, 0, false, 0.01f},     // Music: never stolen
    };

    // Act
    const i32 pick = ChooseVoiceToSteal(voices, 128, 0, false);

    // Assert
    ENJIN_EXPECT_EQ(pick, 1);
}

ENJIN_TEST(VoiceBudget, test_voice_budget_more_important_voices_refuse_new_sound) {
    // Arrange: only dialogue playing (priority 32); a footstep (128) asks for a voice
    std::vector<VoiceCandidate> voices = {{3, 32, false, 0.1f}, {3, 32, false, 0.3f}};

    // Act
    const i32 forFootstep = ChooseVoiceToSteal(voices, 128, 0, false);
    const i32 forDialogue = ChooseVoiceToSteal(voices, 32, 3, false);

    // Assert: the footstep is refused; an equal-priority line takes the quieter voice
    ENJIN_EXPECT_EQ(forFootstep, -1);
    ENJIN_EXPECT_EQ(forDialogue, 0);
}

ENJIN_TEST(VoiceBudget, test_voice_budget_channel_cap_steals_within_channel) {
    // Arrange: a quiet UI click and a louder SFX; the Voice channel is full
    std::vector<VoiceCandidate> voices = {{2, 64, false, 0.05f}, {3, 32, false, 0.5f}};

    // Act
    const i32 pick = ChooseVoiceToSteal(voices, 32, 3, /*sameChannelOnly=*/true);

    // Assert: only a Voice-channel sound can make room under the Voice cap
    ENJIN_EXPECT_EQ(pick, 1);
}

ENJIN_TEST(VoiceBudget, test_voice_budget_priority_channel_default_and_override) {
    // Arrange / Act / Assert
    ENJIN_EXPECT_EQ(ResolveVoicePriority(kUseChannelPriority, 3), 32);   // Voice
    ENJIN_EXPECT_EQ(ResolveVoicePriority(kUseChannelPriority, 0), 128);  // SFX
    ENJIN_EXPECT_EQ(ResolveVoicePriority(10, 0), 10);                    // authored
}

ENJIN_TEST(MIDI, test_midi_inject_message_readable_after_update) {
    // Arrange: every backend (winmm, Web MIDI) feeds raw messages through
    // InjectMessage, so this is the path a browser's note takes
    MIDIInput midi;

    // Act
    midi.InjectMessage(0x91, 60, 100);   // note on, channel 1
    midi.InjectMessage(0xB0, 7, 90);     // CC 7, channel 0
    midi.InjectMessage(0xF8, 0, 0);      // clock: dropped
    const bool beforeUpdate = midi.IsNoteOn(60);
    midi.Update();

    // Assert
    ENJIN_EXPECT_FALSE(beforeUpdate);
    ENJIN_EXPECT_TRUE(midi.IsNoteOn(60, 1));
    ENJIN_EXPECT_EQ(midi.GetNoteVelocity(60), 100u);
    ENJIN_EXPECT_EQ(midi.GetCCValue(7, 0), 90u);
    ENJIN_EXPECT_EQ(midi.GetEvents().size(), (size_t)2);
}

// ===========================================================================
// AudioEventGraph Types
// ===========================================================================

ENJIN_TEST(AudioGraph, NodeTypeCount) {
    // Verify a few key node types exist
    ENJIN_EXPECT_EQ((int)AudioNodeType::EventTrigger, 0);
    ENJIN_EXPECT_EQ((int)AudioNodeType::SoundClip, 2);
    ENJIN_EXPECT_EQ((int)AudioNodeType::Volume, 5);
    ENJIN_EXPECT_EQ((int)AudioNodeType::MasterOutput, 13);
}

ENJIN_TEST(AudioGraph, NodeDefaults) {
    AudioGraphNode node;
    ENJIN_EXPECT_EQ(node.id, 0u);
    ENJIN_EXPECT_EQ((int)node.type, (int)AudioNodeType::SoundClip);
    ENJIN_EXPECT_FLOAT_EQ(node.floatValue, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(node.minValue, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(node.maxValue, 1.0f);
}

ENJIN_TEST(AudioGraph, DataDefaults) {
    AudioEventGraphData data;
    ENJIN_EXPECT_STR_EQ(data.name.c_str(), "New Audio Event");
    ENJIN_EXPECT_EQ(data.nodes.size(), (size_t)0);
    ENJIN_EXPECT_EQ(data.links.size(), (size_t)0);
    ENJIN_EXPECT_EQ(data.nextNodeId, 1u);
    ENJIN_EXPECT_EQ(data.nextLinkId, 1u);
}

// SD-23: the accessibility indicator captioned every sound with its file name,
// and the authored AudioSourceComponent::audioDescription was read only by its
// own inspector. The description wins when there is one.
ENJIN_TEST(AudioCaptions, TheAuthoredDescriptionIsTheCaption) {
    using Enjin::Audio::AudioEngine;
    ENJIN_EXPECT_STR_EQ(AudioEngine::SoundCaption("Door creaks open", "assets/sfx/door_03.wav").c_str(),
                        "Door creaks open");
    // No description: the file name, without its folders, as before.
    ENJIN_EXPECT_STR_EQ(AudioEngine::SoundCaption("", "assets/sfx/door_03.wav").c_str(), "door_03.wav");
    ENJIN_EXPECT_STR_EQ(AudioEngine::SoundCaption("", "C:\\game\\bell.ogg").c_str(), "bell.ogg");
}

// SD-24: an authored viseme track was saved and shown and read by nothing,
// so it never moved a mouth. The key in effect at the sound's playback time is
// what drives the current viseme now.
ENJIN_TEST(LipSync, TheTrackGivesTheKeyInEffectAtATime) {
    using LS = Enjin::ECS::LipSyncComponent;
    using V = Enjin::ECS::Viseme;
    // Arrange: out of order on purpose.
    std::vector<LS::VisemeKey> keys(3);
    keys[0].time = 0.50f; keys[0].viseme = V::OH; keys[0].weight = 0.8f;
    keys[1].time = 0.10f; keys[1].viseme = V::AA; keys[1].weight = 1.0f;
    keys[2].time = 0.90f; keys[2].viseme = V::Silent; keys[2].weight = 0.0f;
    V v; Enjin::f32 w;

    // Act / Assert
    LS::SampleTrack(keys, 0.05f, v, w);   // before the first key
    ENJIN_EXPECT_TRUE(v == V::Silent && w == 0.0f);
    LS::SampleTrack(keys, 0.30f, v, w);
    ENJIN_EXPECT_TRUE(v == V::AA && w == 1.0f);
    LS::SampleTrack(keys, 0.50f, v, w);   // exactly on a key
    ENJIN_EXPECT_TRUE(v == V::OH && w == 0.8f);
    LS::SampleTrack(keys, 2.00f, v, w);   // after the last key
    ENJIN_EXPECT_TRUE(v == V::Silent);
}

ENJIN_TEST_MAIN()
