// EditorLayerComponents_Audio.cpp — Audio component inspector draw functions
// Split from EditorLayerComponents.cpp for faster incremental builds.
#include "Enjin/Editor/EditorLayer.h"
#include "Enjin/Editor/EditorTheme.h"
#include "Enjin/Editor/InspectorUndo.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/Math/Math.h"

namespace Enjin {

using namespace Editor;

void EditorLayer::DrawAudioSourceComponent(ECS::Entity entity) {
    bool audioOpen = ImGui::CollapsingHeader("[A] Audio Source", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("AudioSourceCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::AudioSourceComponent>(entity, "audioSource", "Audio Source");
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (audioOpen) {
        auto* audio = m_World->GetComponent<ECS::AudioSourceComponent>(entity);
        if (!audio) return;

        // Clip path (would need file browser in real implementation)
        char pathBuffer[256];
        strncpy(pathBuffer, audio->clipPath.c_str(), sizeof(pathBuffer) - 1);
        pathBuffer[sizeof(pathBuffer) - 1] = '\0';
        if (InspectorUndo::InputText(m_UndoRedo, "Clip Path", pathBuffer, sizeof(pathBuffer),
                [audio](const std::string& val) { audio->clipPath = val; })) {
            audio->clipPath = pathBuffer;
        }

        InspectorUndo::DragFloat(m_UndoRedo, "Volume", &audio->volume, 0.01f, 0.0f, 1.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Pitch", &audio->pitch, 0.01f, 0.1f, 3.0f);

        // Audio channel dropdown
        {
            const char* channelNames[] = {"SFX", "Music", "UI", "Voice"};
            int channelIdx = static_cast<int>(audio->channel);
            if (channelIdx < 0 || channelIdx >= 4) channelIdx = 0;
            if (InspectorUndo::Combo(m_UndoRedo, "Channel", &channelIdx, channelNames, 4)) {
                audio->channel = static_cast<ECS::AudioChannel>(channelIdx);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("SFX: in-world sounds\nMusic: background score (always 2D)\nUI: menu sounds (always 2D)\nVoice: dialogue");
            }
        }

        InspectorUndo::Checkbox(m_UndoRedo, "Play On Awake", &audio->playOnAwake);
        InspectorUndo::Checkbox(m_UndoRedo, "Loop", &audio->loop);

        // Music and UI channels force 2D — show is3D only for SFX and Voice
        bool channelForces2D = audio->channel == ECS::AudioChannel::Music || audio->channel == ECS::AudioChannel::UI;
        if (channelForces2D) {
            ImGui::BeginDisabled();
            bool forced2D = false;
            ImGui::Checkbox("3D Sound", &forced2D);
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip("Music and UI channels are always non-diegetic (2D)");
            }
        } else {
            InspectorUndo::Checkbox(m_UndoRedo, "3D Sound", &audio->is3D);
        }

        if (audio->is3D && !channelForces2D) {
            InspectorUndo::DragFloat(m_UndoRedo, "Spatial Blend", &audio->spatialBlend, 0.05f, 0.0f, 1.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Min Distance", &audio->minDistance, 0.5f, 0.1f, 100.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Max Distance", &audio->maxDistance, 5.0f, audio->minDistance, 1000.0f);
        }

        InspectorUndo::DragInt(m_UndoRedo, "Priority", &audio->priority, 1, 0, 255);

        // Sound randomization
        if (ImGui::TreeNode("Randomization")) {
            InspectorUndo::DragFloat(m_UndoRedo, "Pitch Min##Rand", &audio->pitchMin, 0.01f, 0.1f, 3.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Pitch Max##Rand", &audio->pitchMax, 0.01f, 0.1f, 3.0f);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Random pitch variation per play (1.0 = no variation)");
            InspectorUndo::DragFloat(m_UndoRedo, "Volume Min##Rand", &audio->volumeMin, 0.01f, 0.0f, 2.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Volume Max##Rand", &audio->volumeMax, 0.01f, 0.0f, 2.0f);
            InspectorUndo::Checkbox(m_UndoRedo, "No Repeat##Rand", &audio->noRepeat);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Avoid playing the same clip variation twice in a row");
            ImGui::Text("Clip Variations: %zu", audio->clipVariations.size());
            ImGui::TreePop();
        }

        // Sound pooling
        if (ImGui::TreeNode("Pooling")) {
            InspectorUndo::Checkbox(m_UndoRedo, "Use Pooling##Pool", &audio->usePooling);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Pre-allocate sound voices for rapid-fire SFX (gunshots, footsteps)");
            if (audio->usePooling) {
                int ps = static_cast<int>(audio->poolSize);
                if (ImGui::InputInt("Pool Size##Pool", &ps)) audio->poolSize = static_cast<u32>(Math::Max(1, ps));
            }
            ImGui::TreePop();
        }

        // Accessibility
        if (ImGui::TreeNode("Accessibility##Audio")) {
            char descBuf[256] = {};
            strncpy(descBuf, audio->audioDescription.c_str(), sizeof(descBuf) - 1);
            if (ImGui::InputText("Description##ADesc", descBuf, sizeof(descBuf))) audio->audioDescription = descBuf;
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Text description of this sound for audio-impaired users");
            ImGui::TreePop();
        }

        ImGui::Separator();
        ImGui::Text("Playing: %s", audio->isPlaying ? "Yes" : "No");

        if (ImGui::Button("Play")) {
            audio->isPlaying = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Stop")) {
            audio->isPlaying = false;
            audio->playbackPosition = 0.0f;
        }
    }
}


void EditorLayer::DrawAudioListenerComponent(ECS::Entity entity) {
    if (ImGui::CollapsingHeader("Audio Listener", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto* listener = m_World->GetComponent<ECS::AudioListenerComponent>(entity);
        if (!listener) return;

        InspectorUndo::Checkbox(m_UndoRedo, "Active", &listener->isActive);
        InspectorUndo::DragFloat(m_UndoRedo, "Volume Scale", &listener->volumeScale, 0.01f, 0.0f, 2.0f);

        if (ImGui::BeginPopupContextItem("AudioListenerContext")) {
            if (ImGui::MenuItem("Remove Component")) {
                RemoveComponentWithUndo<ECS::AudioListenerComponent>(entity, "audioListener", "Audio Listener");
            }
            ImGui::EndPopup();
        }
    }
}

// ============================================================================
// Advanced Audio Components
// ============================================================================

void EditorLayer::DrawReverbZoneComponent(ECS::Entity entity) {
    bool open = ImGui::CollapsingHeader("[~] Reverb Zone", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("ReverbZoneCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::ReverbZoneComponent>(entity, "reverbZone", "Reverb Zone");
            ImGui::EndPopup(); return;
        }
        ImGui::EndPopup();
    }
    if (open) {
        auto* rz = m_World->GetComponent<ECS::ReverbZoneComponent>(entity);
        if (!rz) return;

        InspectorUndo::Checkbox(m_UndoRedo, "Active##RZ", &rz->isActive);
        InspectorUndo::Checkbox(m_UndoRedo, "Global##RZ", &rz->isGlobal);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Scene-wide default reverb (ignores shape)");

        const char* presetNames[] = {"Custom","Small Room","Large Room","Hall","Cathedral","Cave","Outdoors","Bathroom","Under Water"};
        int preset = static_cast<int>(rz->preset);
        if (ImGui::Combo("Preset##RZ", &preset, presetNames, 9)) {
            rz->preset = static_cast<ECS::ReverbZoneComponent::Preset>(preset);
        }

        ImGui::DragFloat3("Half Extents##RZ", &rz->halfExtents.x, 0.5f, 0.1f, 100.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Blend Radius##RZ", &rz->blendRadius, 0.1f, 0.0f, 20.0f);
        ImGui::InputInt("Priority##RZ", &rz->priority);

        if (ImGui::TreeNode("Reverb Parameters")) {
            InspectorUndo::DragFloat(m_UndoRedo, "Room Size", &rz->roomSize, 0.01f, 0.0f, 1.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Damping", &rz->damping, 0.01f, 0.0f, 1.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Wet/Dry Mix", &rz->wetDryMix, 0.01f, 0.0f, 1.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Decay Time", &rz->decayTime, 0.1f, 0.1f, 20.0f);
            InspectorUndo::DragFloat(m_UndoRedo, "Pre-Delay", &rz->preDelay, 0.001f, 0.0f, 0.1f);
            ImGui::TreePop();
        }
    }
}

void EditorLayer::DrawAmbientSoundLayerComponent(ECS::Entity entity) {
    bool open = ImGui::CollapsingHeader("[A] Ambient Sound Layer", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("AmbientSoundCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::AmbientSoundLayerComponent>(entity, "ambientSoundLayer", "Ambient Sound Layer");
            ImGui::EndPopup(); return;
        }
        ImGui::EndPopup();
    }
    if (open) {
        auto* asl = m_World->GetComponent<ECS::AmbientSoundLayerComponent>(entity);
        if (!asl) return;

        InspectorUndo::Checkbox(m_UndoRedo, "Active##ASL", &asl->isActive);
        ImGui::DragFloat3("Half Extents##ASL", &asl->halfExtents.x, 0.5f, 0.1f, 100.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Blend Radius##ASL", &asl->blendRadius, 0.5f, 0.0f, 50.0f);

        ImGui::Separator();
        ImGui::Text("Layers (%zu):", asl->layers.size());

        i32 removeIdx = -1;
        for (usize i = 0; i < asl->layers.size(); ++i) {
            auto& layer = asl->layers[i];
            ImGui::PushID(static_cast<int>(i));

            char clipBuf[256] = {};
            strncpy(clipBuf, layer.clipPath.c_str(), sizeof(clipBuf) - 1);
            ImGui::SetNextItemWidth(200.0f);
            if (ImGui::InputText("Clip##ASLClip", clipBuf, sizeof(clipBuf))) layer.clipPath = clipBuf;
            ImGui::SameLine();
            InspectorUndo::DragFloat(m_UndoRedo, "Vol##ASLVol", &layer.volume, 0.05f, 0.0f, 2.0f);

            char captionBuf[128] = {};
            strncpy(captionBuf, layer.caption.c_str(), sizeof(captionBuf) - 1);
            if (ImGui::InputText("Caption##ASLCap", captionBuf, sizeof(captionBuf))) layer.caption = captionBuf;
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Accessibility: shown as subtitle when layer activates (e.g. \"[Birds chirping]\")");

            ImGui::SameLine();
            if (ImGui::SmallButton("X##ASLRemove")) removeIdx = static_cast<i32>(i);

            ImGui::Separator();
            ImGui::PopID();
        }
        if (removeIdx >= 0) asl->layers.erase(asl->layers.begin() + removeIdx);
        if (ImGui::Button("Add Layer##ASL")) {
            asl->layers.push_back({});
        }
    }
}

void EditorLayer::DrawMusicZoneComponent(ECS::Entity entity) {
    bool open = ImGui::CollapsingHeader("[M] Music Zone", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("MusicZoneCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::MusicZoneComponent>(entity, "musicZone", "Music Zone");
            ImGui::EndPopup(); return;
        }
        ImGui::EndPopup();
    }
    if (open) {
        auto* mz = m_World->GetComponent<ECS::MusicZoneComponent>(entity);
        if (!mz) return;

        InspectorUndo::Checkbox(m_UndoRedo, "Active##MZ", &mz->isActive);

        char trackBuf[256] = {};
        strncpy(trackBuf, mz->trackPath.c_str(), sizeof(trackBuf) - 1);
        if (ImGui::InputText("Track##MZ", trackBuf, sizeof(trackBuf))) mz->trackPath = trackBuf;

        InspectorUndo::DragFloat(m_UndoRedo, "Fade In##MZ", &mz->fadeInTime, 0.1f, 0.0f, 10.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Fade Out##MZ", &mz->fadeOutTime, 0.1f, 0.0f, 10.0f);
        ImGui::DragFloat3("Half Extents##MZ", &mz->halfExtents.x, 0.5f, 0.1f, 100.0f);
        ImGui::InputInt("Priority##MZ", &mz->priority);
    }
}

void EditorLayer::DrawAudioSnapshotTriggerComponent(ECS::Entity entity) {
    bool open = ImGui::CollapsingHeader("[S] Audio Snapshot Trigger", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("SnapshotTrigCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::AudioSnapshotTriggerComponent>(entity, "audioSnapshotTrigger", "Audio Snapshot Trigger");
            ImGui::EndPopup(); return;
        }
        ImGui::EndPopup();
    }
    if (open) {
        auto* st = m_World->GetComponent<ECS::AudioSnapshotTriggerComponent>(entity);
        if (!st) return;

        InspectorUndo::Checkbox(m_UndoRedo, "Active##ST", &st->isActive);

        char snapBuf[128] = {};
        strncpy(snapBuf, st->snapshotName.c_str(), sizeof(snapBuf) - 1);
        if (ImGui::InputText("Snapshot##ST", snapBuf, sizeof(snapBuf))) st->snapshotName = snapBuf;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Built-in: Dialogue, Pause, Combat, Cutscene\nOr use a custom name.");

        ImGui::DragFloat3("Half Extents##ST", &st->halfExtents.x, 0.5f, 0.1f, 100.0f);

        // Status
        ImGui::TextDisabled("Listener inside: %s", st->listenerInside ? "Yes" : "No");
    }
}

void EditorLayer::DrawAudioOcclusionComponent(ECS::Entity entity) {
    bool open = ImGui::CollapsingHeader("[O] Audio Occlusion", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("OcclusionCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::AudioOcclusionComponent>(entity, "audioOcclusion", "Audio Occlusion");
            ImGui::EndPopup(); return;
        }
        ImGui::EndPopup();
    }
    if (open) {
        auto* occ = m_World->GetComponent<ECS::AudioOcclusionComponent>(entity);
        if (!occ) return;

        InspectorUndo::Checkbox(m_UndoRedo, "Enabled##Occ", &occ->enabled);
        InspectorUndo::DragFloat(m_UndoRedo, "LowPass Cutoff##Occ", &occ->lowPassCutoff, 10.0f, 100.0f, 20000.0f);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Frequency cutoff when fully occluded (Hz). Lower = more muffled.");
        InspectorUndo::DragFloat(m_UndoRedo, "Volume Reduction##Occ", &occ->volumeReduction, 0.05f, 0.0f, 1.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Update Rate##Occ", &occ->updateRate, 1.0f, 1.0f, 60.0f);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Raycast check frequency (Hz). Lower = cheaper, less responsive.");

        // Runtime status
        ImGui::Separator();
        ImGui::ProgressBar(occ->occlusionAmount, ImVec2(-1, 0), "Occlusion");
    }
}

void EditorLayer::DrawLipSyncComponent(ECS::Entity entity) {
    bool open = ImGui::CollapsingHeader("[L] Lip Sync", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("LipSyncCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::LipSyncComponent>(entity, "lipSync", "Lip Sync");
            ImGui::EndPopup(); return;
        }
        ImGui::EndPopup();
    }
    if (open) {
        auto* ls = m_World->GetComponent<ECS::LipSyncComponent>(entity);
        if (!ls) return;

        InspectorUndo::DragFloat(m_UndoRedo, "Blend Speed##LS", &ls->blendSpeed, 0.5f, 1.0f, 30.0f);
        InspectorUndo::Checkbox(m_UndoRedo, "Auto from Amplitude##LS", &ls->autoFromAmplitude);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Fallback: use audio amplitude to drive mouth open/close.\nNo viseme data needed.");

        // Current viseme display
        static const char* visemeNames[] = {"Silent","PP","FF","TH","DD","KK","CH","SS","NN","RR","AA","EE","IH","OH","OU"};
        int vIdx = static_cast<int>(ls->currentViseme);
        if (vIdx >= 0 && vIdx < 15) {
            ImGui::Text("Current: %s (%.0f%%)", visemeNames[vIdx], ls->currentWeight * 100.0f);
        }

        ImGui::Text("Viseme Keys: %zu", ls->visemeData.size());
    }
}

// ============================================================================
// Audio-Reactive Components
// ============================================================================

void EditorLayer::DrawAudioReactiveComponent(ECS::Entity entity) {
    bool open = ImGui::CollapsingHeader("[~] Audio Reactive", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("AudioReactiveCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::AudioReactiveComponent>(entity, "audioReactive", "Audio Reactive");
            ImGui::EndPopup(); return;
        }
        ImGui::EndPopup();
    }
    if (open) {
        auto* ar = m_World->GetComponent<ECS::AudioReactiveComponent>(entity);
        if (!ar) return;

        InspectorUndo::Checkbox(m_UndoRedo, "Enabled##AR", &ar->enabled);

        char busBuf[64] = {};
        strncpy(busBuf, ar->busName.c_str(), sizeof(busBuf) - 1);
        if (ImGui::InputText("Bus##AR", busBuf, sizeof(busBuf))) ar->busName = busBuf;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Which audio bus to read level from (SFX, Music, UI, Voice, or custom)");

        const char* targetNames[] = {"Light Intensity","Light Color","Emissive Strength","Scale","Opacity","Particle Rate","Camera Shake","Custom Event"};
        int target = static_cast<int>(ar->target);
        if (ImGui::Combo("Target##AR", &target, targetNames, 8)) ar->target = static_cast<ECS::AudioTargetProperty>(target);

        InspectorUndo::DragFloat(m_UndoRedo, "Threshold##AR", &ar->threshold, 0.01f, 0.0f, 1.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Multiplier##AR", &ar->multiplier, 0.1f, 0.0f, 10.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Smoothing##AR", &ar->smoothing, 0.5f, 0.1f, 50.0f);
        InspectorUndo::Checkbox(m_UndoRedo, "Invert##AR", &ar->invert);
        InspectorUndo::DragFloat(m_UndoRedo, "Base Value##AR", &ar->baseValue, 0.1f, 0.0f, 100.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Max Value##AR", &ar->maxValue, 0.1f, 0.0f, 100.0f);

        ImGui::Separator();
        ImGui::ProgressBar(ar->currentValue / Math::Max(ar->maxValue, 0.01f), ImVec2(-1, 0), "Level");
    }
}

void EditorLayer::DrawAudioThresholdTriggerComponent(ECS::Entity entity) {
    bool open = ImGui::CollapsingHeader("[!] Audio Threshold Trigger", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("ThreshTrigCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::AudioThresholdTriggerComponent>(entity, "audioThresholdTrigger", "Audio Threshold Trigger");
            ImGui::EndPopup(); return;
        }
        ImGui::EndPopup();
    }
    if (open) {
        auto* trig = m_World->GetComponent<ECS::AudioThresholdTriggerComponent>(entity);
        if (!trig) return;

        InspectorUndo::Checkbox(m_UndoRedo, "Enabled##ATT", &trig->enabled);

        char busBuf[64] = {};
        strncpy(busBuf, trig->busName.c_str(), sizeof(busBuf) - 1);
        if (ImGui::InputText("Bus##ATT", busBuf, sizeof(busBuf))) trig->busName = busBuf;

        InspectorUndo::DragFloat(m_UndoRedo, "Threshold##ATT", &trig->threshold, 0.01f, 0.0f, 1.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Cooldown##ATT", &trig->cooldown, 0.1f, 0.0f, 5.0f);

        const char* actionNames[] = {"Flicker Lights","Camera Shake","Particle Burst","Fire Event"};
        int action = static_cast<int>(trig->action);
        if (ImGui::Combo("Action##ATT", &action, actionNames, 4)) trig->action = static_cast<ECS::AudioThresholdTriggerComponent::Action>(action);

        InspectorUndo::DragFloat(m_UndoRedo, "Duration##ATT", &trig->effectDuration, 0.05f, 0.05f, 5.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Intensity##ATT", &trig->effectIntensity, 0.1f, 0.0f, 10.0f);

        ImGui::Separator();
        ImGui::Text("Status: %s", trig->triggered ? "TRIGGERED" : (trig->cooldownTimer > 0.0f ? "COOLDOWN" : "Ready"));
    }
}

void EditorLayer::DrawRTPCComponent(ECS::Entity entity) {
    bool open = ImGui::CollapsingHeader("[P] RTPC (Parameter Control)", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("RTPCCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::RTPCComponent>(entity, "rtpc", "RTPC");
            ImGui::EndPopup(); return;
        }
        ImGui::EndPopup();
    }
    if (open) {
        auto* rtpc = m_World->GetComponent<ECS::RTPCComponent>(entity);
        if (!rtpc) return;

        InspectorUndo::Checkbox(m_UndoRedo, "Enabled##RTPC", &rtpc->enabled);

        ImGui::Text("Mappings (%zu):", rtpc->mappings.size());
        i32 removeIdx = -1;
        for (usize i = 0; i < rtpc->mappings.size(); ++i) {
            auto& m = rtpc->mappings[i];
            ImGui::PushID(static_cast<int>(i));

            char paramBuf[64] = {};
            strncpy(paramBuf, m.parameterName.c_str(), sizeof(paramBuf) - 1);
            ImGui::SetNextItemWidth(100.0f);
            if (ImGui::InputText("##Param", paramBuf, sizeof(paramBuf))) m.parameterName = paramBuf;
            ImGui::SameLine();

            const char* targets[] = {"Volume","Pitch","LowPass","Reverb"};
            int t = static_cast<int>(m.audioTarget);
            ImGui::SetNextItemWidth(80.0f);
            if (ImGui::Combo("##Target", &t, targets, 4)) m.audioTarget = static_cast<ECS::RTPCComponent::Mapping::AudioTarget>(t);
            ImGui::SameLine();
            if (ImGui::SmallButton("X##Rm")) removeIdx = static_cast<i32>(i);

            ImGui::DragFloatRange2("Range##R", &m.paramMin, &m.paramMax, 0.01f, -100.0f, 100.0f);
            ImGui::DragFloatRange2("Output##O", &m.outputMin, &m.outputMax, 0.01f, -100.0f, 100.0f);

            ImGui::Separator();
            ImGui::PopID();
        }
        if (removeIdx >= 0) rtpc->mappings.erase(rtpc->mappings.begin() + removeIdx);
        if (ImGui::Button("Add Mapping##RTPC")) rtpc->mappings.push_back({});

        // Show current parameter values
        if (!rtpc->parameters.empty()) {
            ImGui::Separator();
            ImGui::TextDisabled("Parameters:");
            for (auto& [name, val] : rtpc->parameters) {
                ImGui::Text("  %s = %.2f", name.c_str(), val);
            }
        }
    }
}

void EditorLayer::DrawBeatClockComponent(ECS::Entity entity) {
    bool open = ImGui::CollapsingHeader("[B] Beat Clock", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("BeatClockCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::BeatClockComponent>(entity, "beatClock", "Beat Clock");
            ImGui::EndPopup(); return;
        }
        ImGui::EndPopup();
    }
    if (open) {
        auto* clock = m_World->GetComponent<ECS::BeatClockComponent>(entity);
        if (!clock) return;

        InspectorUndo::Checkbox(m_UndoRedo, "Playing##BC", &clock->playing);
        InspectorUndo::DragFloat(m_UndoRedo, "BPM##BC", &clock->bpm, 1.0f, 20.0f, 300.0f);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Beats per minute. Tap tempo: set manually or use MIDI clock.");
        ImGui::InputInt("Beats/Bar##BC", &clock->beatsPerBar);
        if (clock->beatsPerBar < 1) clock->beatsPerBar = 1;

        // Visual beat indicator
        ImGui::Separator();
        ImGui::Text("Bar %u | Beat %u/%d | Total: %u",
            clock->currentBar + 1, clock->currentBeat + 1, clock->beatsPerBar, clock->totalBeats);

        // Beat dots
        for (i32 i = 0; i < clock->beatsPerBar && i < 16; ++i) {
            if (i > 0) ImGui::SameLine();
            bool active = (static_cast<i32>(clock->currentBeat) == i);
            ImVec4 col = active ? ImVec4(1.0f, 0.8f, 0.2f, 1.0f) : ImVec4(0.3f, 0.3f, 0.3f, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Button, col);
            char label[8]; snprintf(label, sizeof(label), "%d", i + 1);
            ImGui::SmallButton(label);
            ImGui::PopStyleColor();
        }

        f32 phase = clock->GetBeatPhase();
        ImGui::ProgressBar(phase, ImVec2(-1, 4), "");
    }
}

void EditorLayer::DrawBeatSyncComponent(ECS::Entity entity) {
    bool open = ImGui::CollapsingHeader("[S] Beat Sync", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("BeatSyncCtx")) {
        if (ImGui::MenuItem("Remove Component")) {
            RemoveComponentWithUndo<ECS::BeatSyncComponent>(entity, "beatSync", "Beat Sync");
            ImGui::EndPopup(); return;
        }
        ImGui::EndPopup();
    }
    if (open) {
        auto* sync = m_World->GetComponent<ECS::BeatSyncComponent>(entity);
        if (!sync) return;

        InspectorUndo::Checkbox(m_UndoRedo, "Enabled##BS", &sync->enabled);

        const char* modeNames[] = {"Every Beat","Every Downbeat","Every N Beats","Continuous (Sine)"};
        int mode = static_cast<int>(sync->mode);
        if (ImGui::Combo("Mode##BS", &mode, modeNames, 4)) sync->mode = static_cast<ECS::BeatSyncComponent::SyncMode>(mode);

        if (sync->mode == ECS::BeatSyncComponent::SyncMode::EveryNBeats) {
            int div = static_cast<int>(sync->beatDivisor);
            if (ImGui::InputInt("Every N##BS", &div)) sync->beatDivisor = static_cast<u32>(Math::Max(1, div));
        }

        const char* targetNames[] = {"Light Intensity","Light Color","Emissive Strength","Scale","Opacity","Particle Rate","Camera Shake","Custom"};
        int target = static_cast<int>(sync->target);
        if (ImGui::Combo("Target##BS", &target, targetNames, 8)) sync->target = static_cast<ECS::AudioTargetProperty>(target);

        InspectorUndo::DragFloat(m_UndoRedo, "Base Value##BS", &sync->baseValue, 0.1f, 0.0f, 100.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Pulse Value##BS", &sync->pulseValue, 0.1f, 0.0f, 100.0f);
        InspectorUndo::DragFloat(m_UndoRedo, "Decay Speed##BS", &sync->decaySpeed, 0.5f, 0.1f, 50.0f);

        ImGui::Separator();
        ImGui::ProgressBar(sync->currentValue / Math::Max(sync->pulseValue, 0.01f), ImVec2(-1, 0), "Pulse");
    }
}

} // namespace Enjin
