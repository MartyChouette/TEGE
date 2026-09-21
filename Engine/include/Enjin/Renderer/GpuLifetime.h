#pragma once

namespace Enjin {
namespace ECS { class RenderSystem; }
namespace Renderer {

// Proof that the caller is inside the frame's GPU-safe point.
//
// THE RULE IT ENFORCES. Destroying or recreating a GPU object, or updating a
// descriptor set, is only safe at one moment in the frame: before any command
// buffer for this frame has been recorded. `RenderSystem::FlushPendingChanges`
// is that moment, and every runtime calls it once per frame. Do it anywhere else
// and the driver access-violates at SUBMIT -- not at the call, which is what
// makes the crash so hard to attribute: the stack you get is the submit, and the
// code that broke it ran hundreds of lines earlier.
//
// WHY A TYPE AND NOT A COMMENT. It was a comment, on every function that carries
// the rule, and comments do not survive being called from a new place. The
// editor is the standing hazard: `EditorLayer` records its offscreen binds BEFORE
// `World::Update`, so an ImGui panel that toggles a render feature is running
// mid-recording while looking exactly like ordinary UI code. `FlushPendingChanges`
// already early-returns on `m_SkipMainPassRendering` for that reason -- this is
// the other half, stopping the call from being written rather than making it a
// no-op when it is.
//
// HOW IT WORKS. The constructor is private and `RenderSystem` is its only friend,
// so a token can only come into existence inside RenderSystem, and by convention
// only at the top of `FlushPendingChanges`. It is non-copyable and non-movable,
// so one cannot be squirrelled away in a member and produced later; a function
// that wants one takes it by const reference and can only have been handed it
// down the call chain from the safe point.
//
// It is a capability, not a runtime check: it costs nothing, and the failure it
// prevents is a compile error rather than a crash report from a player.
//
// WHAT IT DOES NOT CLAIM. It proves WHERE you are, not that what you are doing is
// safe. Holding one does not make it acceptable to destroy an object the
// PREVIOUS frame's submit may still be reading -- that is what the buffer
// graveyard and `m_FlushTick` are for.
class GpuLifetimeToken {
public:
    GpuLifetimeToken(const GpuLifetimeToken&) = delete;
    GpuLifetimeToken& operator=(const GpuLifetimeToken&) = delete;
    GpuLifetimeToken(GpuLifetimeToken&&) = delete;
    GpuLifetimeToken& operator=(GpuLifetimeToken&&) = delete;

private:
    GpuLifetimeToken() = default;
    friend class ECS::RenderSystem;
};

} // namespace Renderer
} // namespace Enjin
