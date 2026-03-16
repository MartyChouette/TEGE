#include "EnjinTest.h"
#include "Enjin/Audio/SimpleAudio.h"

using namespace Enjin;
using namespace Enjin::Audio;

// ===========================================================================
// AudioChannel Enum
// ===========================================================================

ENJIN_TEST(AudioChannelEnum, Values) {
    ENJIN_EXPECT_EQ((int)AudioChannel::SFX, 0);
    ENJIN_EXPECT_EQ((int)AudioChannel::Music, 1);
    ENJIN_EXPECT_EQ((int)AudioChannel::UI, 2);
    ENJIN_EXPECT_EQ((int)AudioChannel::Voice, 3);
}

ENJIN_TEST(AudioChannelEnum, CountValue) {
    ENJIN_EXPECT_EQ((int)AudioChannel::Count, 4);
}

// ===========================================================================
// Audio Constants
// ===========================================================================

ENJIN_TEST(AudioConstants, InvalidClip) {
    ENJIN_EXPECT_EQ(INVALID_AUDIO_CLIP, 0u);
}

ENJIN_TEST(AudioConstants, InvalidSound) {
    ENJIN_EXPECT_EQ(INVALID_SOUND, 0u);
}

// ===========================================================================
// SoundInstance Defaults
// ===========================================================================

ENJIN_TEST(SoundInstance, ClipDefault) {
    SoundInstance inst;
    ENJIN_EXPECT_EQ(inst.clip, INVALID_AUDIO_CLIP);
}

ENJIN_TEST(SoundInstance, VolumeAndPitch) {
    SoundInstance inst;
    ENJIN_EXPECT_FLOAT_EQ(inst.volume, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(inst.pitch, 1.0f);
}

ENJIN_TEST(SoundInstance, PanDefault) {
    SoundInstance inst;
    ENJIN_EXPECT_FLOAT_EQ(inst.pan, 0.0f);
}

ENJIN_TEST(SoundInstance, LoopAndSpatial) {
    SoundInstance inst;
    ENJIN_EXPECT_FALSE(inst.loop);
    ENJIN_EXPECT_FALSE(inst.is3D);
}

ENJIN_TEST(SoundInstance, ChannelDefault) {
    SoundInstance inst;
    ENJIN_EXPECT_EQ((int)inst.channel, (int)AudioChannel::SFX);
}

ENJIN_TEST(SoundInstance, DistanceDefaults) {
    SoundInstance inst;
    ENJIN_EXPECT_FLOAT_EQ(inst.minDistance, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(inst.maxDistance, 500.0f);
}

ENJIN_TEST(SoundInstance, StateDefaults) {
    SoundInstance inst;
    ENJIN_EXPECT_FALSE(inst.isPlaying);
    ENJIN_EXPECT_FLOAT_EQ(inst.playbackPosition, 0.0f);
}

ENJIN_TEST(SoundInstance, PositionDefault) {
    SoundInstance inst;
    ENJIN_EXPECT_FLOAT_EQ(inst.position.x, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(inst.position.y, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(inst.position.z, 0.0f);
}

ENJIN_TEST(SoundInstance, NullMaSound) {
    SoundInstance inst;
    ENJIN_EXPECT_NULL(inst.maSound);
}

// ===========================================================================
// SimpleAudio Default State
// ===========================================================================

ENJIN_TEST(SimpleAudio, DefaultMasterVolume) {
    SimpleAudio audio;
    ENJIN_EXPECT_FLOAT_EQ(audio.GetMasterVolume(), 1.0f);
}

ENJIN_TEST(SimpleAudio, DefaultChannelVolumes) {
    SimpleAudio audio;
    ENJIN_EXPECT_FLOAT_EQ(audio.GetChannelVolume(AudioChannel::SFX), 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(audio.GetChannelVolume(AudioChannel::Music), 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(audio.GetChannelVolume(AudioChannel::UI), 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(audio.GetChannelVolume(AudioChannel::Voice), 1.0f);
}

ENJIN_TEST_MAIN()
