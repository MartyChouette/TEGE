---
title: "TEGE Player's Manual"
subtitle: "The Enjin Engine, walked through by three people who want different things from it"
date: "2026-09-17"
---

# Before you start

This manual is not a feature list. Feature lists are for people who already know
what they are looking for, and the reference in `docs/USER_MANUAL.md` is a good
one when you get there.

This is the other thing. Three people sit down with TEGE. They want different
things, they have different amounts of experience, and one of them drives the
whole editor without touching a keyboard or a mouse. The manual follows them.

If you recognise yourself in one of them, start there. The other two are worth
reading anyway, because the parts of an engine you never touch are the parts that
decide what your collaborators can do.

## The one rule the engine is built on

Everything in TEGE answers to a single design rule, and it is worth knowing
before you start, because it explains choices that would otherwise look strange:

> **TEGE is a powerful offline tool for people with no AI.**
> A person on their own machine, with no model in the loop and no network, must
> be able to reach every capability through the editor interface.

Two consequences you will actually feel:

1. **There is no "you have to script that" tier.** If a capability exists, there
   is a way to reach it by clicking. Where this manual finds a place that fails
   the rule, it says so out loud rather than teaching you a workaround.
2. **The convenience layers are never the only path.** The engine has an MCP
   server so a model can drive the editor. It is a convenience. Anything that can
   *only* be done through it counts as a missing feature, not a shipped one.

## The three players

**Player One has shipped games before.**
They do not want to be taught what an entity is. They want to know where the
scripting API lives, whether the debugger is worth using, and whether a texture
that came out wrong can be fixed here or has to go back to another program and
come back again. Their journey is about the fast path, and about how few times
they have to leave.

**Player Two has never opened a game engine.**
They start in Creative Mode, which is a rail of thirteen tools and a ground plane.
They build a small platformer where the wind pushes you and you can hover across
a gap. Then they put it on the internet and somebody taps it on a phone. Nothing
in that sentence involves a command line.

**Player Three drives everything with a sip-and-puff switch and a gaze tracker.**
Their journey is the honest one. The engine has real alternative-input support,
with four distinct sip and puff signals and gaze dwell timing that is actually
configurable. It also has a quick-access dial that is perfect for them and that
they cannot currently open. Both facts are in here.

## How to read the boxes

Three kinds of box interrupt the text, and they mean different things.

> **TRY IT**
> Something to do in the editor right now. Every one of these has been checked
> against the engine as it stands.

> **WATCH OUT**
> A place the engine will let you do something that will not work the way you
> expect. These are usually where a session gets lost.

> **THE ENGINE OWES YOU ONE**
> A place where this manual could not honestly tell you to click something,
> because the thing to click does not exist yet. Each one is written up properly
> in `product/ENGINE_FACTS.md` under Gaps. They are in the manual because hiding
> them would make the rest of it less trustworthy.
