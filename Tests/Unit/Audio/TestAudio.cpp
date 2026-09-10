#include "EnjinTest.h"
#include "Enjin/Audio/AudioEngine.h"
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

ENJIN_TEST_MAIN()
