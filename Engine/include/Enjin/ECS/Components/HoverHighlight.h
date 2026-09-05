#pragma once
// Highlight an entity when the cursor is over it.
//
// Every project that lets a player point at something writes this by hand:
// raycast from the cursor, track what changed, then find some way to make the
// thing look picked. The raycast half only started working on web at all this
// week, and the "make it look picked" half has no answer in the engine outside
// the editor's own selection.
//
// This is that, as a component: drop it on an entity, choose a colour, a
// thickness and a style, and it lights up under the cursor in the editor's
// game view, the desktop player and the browser alike.
//
// The drawing goes through the inverted-hull outline pass that already exists
// for cel shading, which is why the thickness is in WORLD units -- it is the
// same number the cel outline uses, and a highlight on a small object needs a
// smaller one.
#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"

namespace Enjin {
namespace ECS {

enum class HighlightStyle : u8 {
    Solid = 0,   // constant outline
    Pulse,       // breathes, for something that wants attention
    Flash        // sharp on/off, for a warning or a forbidden target
};

struct ENJIN_API HoverHighlightComponent {
    // Off means the entity is not highlightable at all -- distinct from
    // "highlightable but not currently hovered", which is `hovered`.
    bool enabled = true;

    // Electric teal by default rather than magenta: magenta reads as a missing
    // texture, and a highlight has to read as intentional at a glance.
    Math::Vector3 color = Math::Vector3(0.1f, 1.0f, 0.8f);

    // World units, same as the cel outline this borrows. Scale-independent by
    // design: colliders are world-space here too, so a highlight that tracked
    // transform scale would disagree with the thing being picked.
    f32 thickness = 0.04f;

    HighlightStyle style = HighlightStyle::Solid;

    // Cycles per second for Pulse and Flash. Ignored by Solid.
    f32 speed = 2.0f;

    // How far Pulse dips. 0 keeps it constant, 1 lets it fade out entirely.
    f32 pulseDepth = 0.5f;

    // Highlight the whole prop when a child is hovered, rather than the one
    // sub-mesh the ray happened to hit. This is what a player expects when
    // pointing at a chair leg.
    bool includeChildren = true;

    // ---- runtime, not serialized ----
    // Set by HoverHighlightSystem each frame. Authored state is everything
    // above; this is the answer to "is the cursor on it right now".
    bool hovered = false;
};

} // namespace ECS
} // namespace Enjin
