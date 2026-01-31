#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include <imgui.h>

namespace Enjin::GUI
{

enum class TransitionType : u8
{
    FadeBlack,
    FadeWhite,
    CircleIris,
    DiamondWipe,
    PixelDissolve,
    SlideLeft,
    SlideRight,
    SlideUp,
    SlideDown
};

enum class TransitionPhase : u8
{
    Idle,
    TransitionOut,
    Hold,
    TransitionIn
};

struct TransitionConfig
{
    TransitionType type = TransitionType::FadeBlack;
    f32 outDuration = 0.5f;
    f32 holdDuration = 0.1f;
    f32 inDuration = 0.5f;
    Math::Vector3 color = Math::Vector3(0.0f, 0.0f, 0.0f);
    u32 pixelSize = 8;
};

class ENJIN_API ScreenTransition
{
public:
    void Start(const TransitionConfig& config);
    void Update(f32 dt);
    void Render(f32 screenW, f32 screenH);

    bool IsActive() const;
    bool IsAtMidpoint() const;
    bool IsComplete() const;
    TransitionPhase GetPhase() const;

private:
    TransitionConfig m_Config;
    TransitionPhase m_Phase = TransitionPhase::Idle;
    f32 m_Timer = 0.0f;
    f32 m_Progress = 0.0f;

    void RenderFade(ImDrawList* dl, f32 w, f32 h, f32 alpha);
    void RenderCircleIris(ImDrawList* dl, f32 w, f32 h, f32 progress, bool closing);
    void RenderDiamondWipe(ImDrawList* dl, f32 w, f32 h, f32 progress, bool closing);
    void RenderPixelDissolve(ImDrawList* dl, f32 w, f32 h, f32 progress, bool closing);
    void RenderSlide(ImDrawList* dl, f32 w, f32 h, f32 progress, bool closing, TransitionType slideDir);
};

} // namespace Enjin::GUI
