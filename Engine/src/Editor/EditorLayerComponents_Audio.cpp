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


} // namespace Enjin
