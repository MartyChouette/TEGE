#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/World.h"
#include <string>

namespace Enjin {
namespace Gameplay {

// The save system's own on-screen feedback: "Game Saved", "Saving...", a save
// point's prompt.
//
// Driven by SaveSystemComponent, which already designs this and always has:
// showSaveIndicator, saveIndicatorDuration, showAutoSaveWarning, and the
// runtime pair isSaving / saveIndicatorTimer. Those fields existed with nothing
// reading them; this is the thing that reads them.
//
// NOT the subtitle system, which is where this briefly lived and did not
// belong. A caption describes a SOUND for someone who cannot hear it and is
// opt-in for that reason -- routing save feedback through it made a save
// invisible to anyone without captions turned on, which is most people. A save
// confirmation is not a caption, it is the save system talking, so it is
// configured by the save component and drawn by the save domain.
//
// Same overlay shape as SubtitleSystem and AudioVisualIndicatorSystem --
// RenderOverlay(originX, originY, w, h), called once per frame by each runtime
// beside the others. The origin is not optional for the same reason it is not
// optional there: in the editor the game is a panel somewhere in the middle of
// the window, and a baked-in 0,0 puts the message over whatever is docked at
// the top-left.
class ENJIN_API SaveIndicator {
public:
    void SetWorld(ECS::World* world) { m_World = world; }

    // Post a message. Duration 0 takes the component's saveIndicatorDuration,
    // which is the setting a person actually edits.
    void Show(const std::string& text, f32 duration = 0.0f);

    // Expire what is showing, and drive SaveSystemComponent's isSaving /
    // saveIndicatorTimer from the same clock so the two cannot disagree.
    void Update(f32 deltaTime);

    void RenderOverlay(f32 originX, f32 originY, u32 viewportWidth, u32 viewportHeight);

    // Observables, for tests. Nothing needs these to draw.
    const std::string& GetText() const { return m_Text; }
    f32 GetRemaining() const { return m_Remaining; }

private:
    ECS::World* m_World = nullptr;
    std::string m_Text;
    f32 m_Remaining = 0.0f;
    f32 m_Duration = 0.0f;
};

} // namespace Gameplay
} // namespace Enjin
