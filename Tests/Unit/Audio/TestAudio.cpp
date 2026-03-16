#include "EnjinTest.h"
#include "Enjin/Audio/AudioSystem.h"
#include "Enjin/Audio/SimpleAudio.h"
#include "Enjin/Input/MIDIInput.h"
#include "Enjin/Editor/AudioEventGraph.h"

#include <cmath>

using namespace Enjin;
using namespace Enjin::Audio;
using namespace Enjin::InputSystem;
using namespace Enjin::Editor;

// ===========================================================================
// SoundSettings Defaults
// ===========================================================================

ENJIN_TEST(SoundSettings, Defaults) {
    SoundSettings s;
    ENJIN_EXPECT_EQ((int)s.type, (int)SoundType::SoundEffect);
    ENJIN_EXPECT_EQ((int)s.loadMode, (int)LoadMode::Memory);
    ENJIN_EXPECT_FALSE(s.loop);
    ENJIN_EXPECT_FLOAT_EQ(s.volume, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(s.pitch, 1.0f);
    ENJIN_EXPECT_FALSE(s.is3D);
    ENJIN_EXPECT_FLOAT_EQ(s.minDistance, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(s.maxDistance, 100.0f);
    ENJIN_EXPECT_EQ((int)s.attenuation, (int)AttenuationMode::InverseDistanceClamped);
}

// ===========================================================================
// AudioListener Defaults
// ===========================================================================

ENJIN_TEST(Listener, Defaults) {
    AudioListener listener;
    ENJIN_EXPECT_FLOAT_EQ(listener.forward.z, -1.0f);
    ENJIN_EXPECT_FLOAT_EQ(listener.up.y, 1.0f);
}

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

// ===========================================================================
// SoundType & AttenuationMode Enums
// ===========================================================================

ENJIN_TEST(Enums, SoundType) {
    ENJIN_EXPECT_EQ((int)SoundType::SoundEffect, 0);
    ENJIN_EXPECT_EQ((int)SoundType::Music, 1);
    ENJIN_EXPECT_EQ((int)SoundType::Ambient, 2);
    ENJIN_EXPECT_EQ((int)SoundType::Voice, 3);
}

ENJIN_TEST(Enums, AttenuationMode) {
    ENJIN_EXPECT_EQ((int)AttenuationMode::None, 0);
    ENJIN_EXPECT_EQ((int)AttenuationMode::Linear, 1);
    ENJIN_EXPECT_EQ((int)AttenuationMode::InverseDistance, 2);
    ENJIN_EXPECT_EQ((int)AttenuationMode::InverseDistanceClamped, 3);
    ENJIN_EXPECT_EQ((int)AttenuationMode::Logarithmic, 4);
}

// ===========================================================================
// Handle Constants
// ===========================================================================

ENJIN_TEST(Handles, InvalidConstants) {
    ENJIN_EXPECT_EQ(INVALID_SOUND, 0u);
    ENJIN_EXPECT_EQ(INVALID_CHANNEL, 0u);
}

// ===========================================================================
// AudioUtils Math
// ===========================================================================

ENJIN_TEST(AudioUtils, DbToLinearZero) {
    f32 linear = AudioUtils::DbToLinear(0.0f);
    ENJIN_EXPECT_FLOAT_NEAR(linear, 1.0f, 0.01f);
}

ENJIN_TEST(AudioUtils, DbToLinearMinus6) {
    // -6dB ≈ 0.5 linear
    f32 linear = AudioUtils::DbToLinear(-6.0f);
    ENJIN_EXPECT_FLOAT_NEAR(linear, 0.5f, 0.05f);
}

ENJIN_TEST(AudioUtils, LinearToDbOne) {
    f32 db = AudioUtils::LinearToDb(1.0f);
    ENJIN_EXPECT_FLOAT_NEAR(db, 0.0f, 0.01f);
}

ENJIN_TEST(AudioUtils, RoundTrip) {
    f32 original = 0.7f;
    f32 db = AudioUtils::LinearToDb(original);
    f32 back = AudioUtils::DbToLinear(db);
    ENJIN_EXPECT_FLOAT_NEAR(back, original, 0.01f);
}

ENJIN_TEST(AudioUtils, Calculate3DVolumeAtMin) {
    // At minDistance, volume should be full (1.0)
    f32 vol = AudioUtils::Calculate3DVolume(1.0f, 1.0f, 100.0f, AttenuationMode::InverseDistanceClamped);
    ENJIN_EXPECT_FLOAT_NEAR(vol, 1.0f, 0.01f);
}

ENJIN_TEST(AudioUtils, Calculate3DVolumeAtMax) {
    // At maxDistance, volume should be near 0
    f32 vol = AudioUtils::Calculate3DVolume(100.0f, 1.0f, 100.0f, AttenuationMode::Linear);
    ENJIN_EXPECT_FLOAT_NEAR(vol, 0.0f, 0.05f);
}

ENJIN_TEST(AudioUtils, Calculate3DVolumeNone) {
    // No attenuation → full volume regardless of distance
    f32 vol = AudioUtils::Calculate3DVolume(50.0f, 1.0f, 100.0f, AttenuationMode::None);
    ENJIN_EXPECT_FLOAT_EQ(vol, 1.0f);
}

ENJIN_TEST(AudioUtils, CrossfadeVolumeEdges) {
    // At progress 0: fadeIn=0, fadeOut=1
    ENJIN_EXPECT_FLOAT_NEAR(AudioUtils::CrossfadeVolume(0.0f, true), 0.0f, 0.01f);
    ENJIN_EXPECT_FLOAT_NEAR(AudioUtils::CrossfadeVolume(0.0f, false), 1.0f, 0.01f);
    // At progress 1: fadeIn=1, fadeOut=0
    ENJIN_EXPECT_FLOAT_NEAR(AudioUtils::CrossfadeVolume(1.0f, true), 1.0f, 0.01f);
    ENJIN_EXPECT_FLOAT_NEAR(AudioUtils::CrossfadeVolume(1.0f, false), 0.0f, 0.01f);
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
