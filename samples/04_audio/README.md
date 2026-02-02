# Sample 04: Audio

Demonstrates the audio system: sound playback, 3D spatialization, footsteps, and scripted audio.

## What This Demonstrates

- AudioSourceComponent for sound playback
- AudioListenerComponent for 3D spatialization
- FootstepSystem for surface-based footstep sounds
- Controlling audio from AngelScript

## Scene Contents

| Entity | Components | Purpose |
|--------|-----------|---------|
| Player | Transform, CharacterController, AudioListener, Footstep, Script | Player with footstep sounds |
| Ambient_Music | Transform, AudioSource, Name | Background music (2D, looping) |
| Waterfall | Transform, AudioSource, Mesh | 3D spatial sound source |
| Button | Transform, Mesh, Interactable, AudioSource | Plays sound on interaction |
| Camera | Transform, Camera | Player camera |

## Audio Configuration

### AudioSourceComponent

| Property | Description |
|----------|-------------|
| `clipPath` | Path to audio file (WAV) |
| `volume` | Playback volume (0.0-1.0) |
| `pitch` | Playback speed/pitch |
| `loop` | Whether to loop playback |
| `playOnAwake` | Auto-play when scene loads |
| `is3D` | Enable 3D spatialization |
| `minDistance` | Distance at which sound is at full volume |
| `maxDistance` | Distance at which sound fades to zero |

### FootstepComponent

Configure per-surface footstep audio:
- `walkInterval` / `runInterval` - Time between steps
- `pitchVariance` - Random pitch variation for natural sound
- Surface material mappings (concrete, grass, wood, metal, etc.)

## Scripted Audio

```angelscript
class AudioController : TegeBehavior {
    void OnCreate() {
        // Play an entity's AudioSource
        Audio_Play(entityId);
    }

    void OnUpdate(float dt) {
        // Control volume dynamically
        Audio_SetVolume(entityId, 0.5f);

        // Play sound at a position
        Audio_PlayAtPosition("audio/explosion.wav", Vector3(10, 0, 5));

        // Check if still playing
        if (!Audio_IsPlaying(entityId)) {
            Audio_Play(entityId);
        }
    }
}
```

## Key Concepts

### 3D Audio

For spatial audio, the listener (usually attached to the player/camera) determines what the player hears. Sound sources with `is3D = true` will attenuate based on distance and pan based on direction.

### Master Volume

```angelscript
Audio_SetMasterVolume(0.8f);  // 80% master volume
float vol = Audio_GetMasterVolume();
```
