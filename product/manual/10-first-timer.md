# Player Two: never opened a game engine

Their whole goal fits in one sentence. Make a small place where the wind pushes
you and you can float across a gap, then send it to a friend who opens it on a
phone and plays it with their thumbs.

They are not going to learn what an entity is first. They are going to make
something and find out what an entity is by accident, later, when they need to
know.

## 1. Opening Creative Mode

Creative Mode is a rail of tools down one side and a ground plane. That is the
whole interface. The thirty-one dockable panels the rest of the editor is made of
still exist and are one click away, and none of them are in the way.

The rail is grouped in three bands, and the grouping is by what a tool makes
rather than by how it works:

| Band | Tools | What the band is for |
|---|---|---|
| Structure | Wall, Floor, Stairs, Path | The parts you walk on and bump into |
| Volume | Brush, Water, Terrain, Cave, Plants | Regions of stuff. Ground, liquid, foliage, the inside of a hill |
| Object | Ladder, Prop, Reduce | Individual things you place |
| On its own | Edit | The only tool that changes what is already there instead of making something new |

`Edit` sits apart on purpose. Everything above it answers the question "what do I
want to add", and it answers "what do I want to change". Keeping that separate is
most of why the rail stays readable at thirteen entries.

> **TRY IT**
> Pick Floor. Press on the ground, drag, release. You have a floor. Nothing
> asked you for a size, a material, a name or a resolution, and you did not open
> a settings window. That is the design rule of Creative Mode working, stated in
> its own header as: someone should be able to block out a playable level and
> press play without ever opening a settings window.

> **WATCH OUT**
> Almost every tool is press, drag, release. `Path` is not. It builds a run of
> connected walls, straight or bowed, and it is the first tool on the rail whose
> gesture is a different shape. If you reach for Path expecting a single drag,
> that is why it feels broken. It is not broken, it is a different verb.

## 2. Blocking out the level

The level is three ideas. A ledge you start on, a gap you cannot jump, and a
ledge on the other side with something worth reaching.

Floor for the two ledges. Wall for anything you want the player to bump into
rather than fall off. Terrain if you would rather the ground rolled a bit, and
Plants once the shape is right, because foliage on a shape you are still changing
is work you will throw away.

> **TRY IT**
> Build both ledges, leave the gap, and press play. Walk to the edge and fall in.
> Falling in is the level working. You now have a problem the rest of the chapter
> solves, which is a better position than having a level with nothing wrong.

Two habits worth forming now, both of which cost nothing today and save a session
later:

- **Block out in grey.** Shape first, surface later. A level that looks finished
  is a level nobody wants to change.
- **Press play constantly.** The gap between building and playing is the whole
  point of this editor. Using it once an hour wastes it.

## 3. Wind

Wind is real and it is a system, not a decoration. `WindSystem` drives cloth and
vegetation, and a `WeatherZone` can override it in a region. A strength of 0 is
how you make a calm pocket indoors while a storm continues outside.

So the grass on both ledges leans, the cloth on the flag over the far ledge
moves, and the place feels like somewhere with weather in it.

> **WATCH OUT**
> Wind moves cloth and plants. It does not push the player. If you were expecting
> to be blown across the gap, that is a different mechanic and it is the subject
> of the next section, which is where this manual stops being able to tell you
> to click something.

## 4. Hover, and where this manual has to be honest

Here is the mechanic the level was built around. Hold a button in the air and
fall slowly, so a gap you cannot jump becomes a gap you can cross if you commit
early enough. It is a good first mechanic because it is one button and the level
teaches it without a word of text.

You cannot currently author it.

> **THE ENGINE OWES YOU ONE**
> `CharacterController` has `airControl` and `coyoteTime`, which are both
> forgiveness parameters of exactly this kind. It has no hover, no glide, no
> descent clamp and no double jump. There is no checkbox and no slider.
>
> Scripting is not a way around it either. Of the nine `Controller_` bindings the
> engine registers, velocity is read-only. There is no `Controller_SetVelocity`,
> no jump control and no per-entity gravity control, so a script cannot reach the
> value it would need to change.
>
> Written up as gap G1 in `product/ENGINE_FACTS.md`.

### The trap you will find while looking for a way around it

You will find `GravityZoneComponent` and it will look like the answer. It is a
box you place in the world with a gravity direction and a strength. Drop it over
the gap, turn the strength down to 2.0, and the player should drift across.

It will do nothing, and nothing will tell you why.

> **WATCH OUT**
> A gravity zone has two modes. `Directional` is the default and it is the one
> you want. Jolt applies it correctly to physics bodies, so a barrel dropped into
> the zone really does float.
>
> The character controller ignores it. `ControllerSystem` only reads gravity
> zones in `Point` mode, and only for the planetary controller. Your player walks
> through a correctly configured, active, serialized zone at completely normal
> gravity, and there is no warning in the log, no mark in the inspector and no
> difference in how the component looks.
>
> The default configuration of the component is the configuration that does
> nothing, for the controller that every platformer uses. Written up as gap G4.

That is worth sitting with for a second, because it is the most useful thing a
first-timer can learn about any engine: **when something does nothing, you have
not necessarily done it wrong.** Check whether the thing you configured is read
by the system you are aiming at. In this case it is read by a different one.

### What to do today

Until G1 and G4 are closed, pick one:

1. **Make the gap jumpable and keep the wind.** The level is still good. Hover
   was one idea in it, not the point of it.
2. **Make the gap a water crossing.** Water is on the rail, it works, and
   swimming is a mechanic the engine actually has.
3. **Put a moving platform in the gap.** Timing instead of float. Different feel,
   same lesson, and it is authorable now.

Option 1 is the right one for a first project. The others are more interesting
and cost more evenings.

## 5. The menu and the pause menu

A game with no menu is a demo. The engine has the screens already, as
`MenuScreen`: MainMenu, PauseMenu, Options, Graphics, Audio, Controls, HowToPlay,
GameOver and LoadGame. You are choosing and arranging, not building from nothing.

Two things to get right the first time.

**Your startup flow is empty, and that is deliberate.** `startupFlow` in the
project file is an ordered list that is not seeded with anything. An empty flow
means the game starts at the game. Nobody is dragged through a splash screen and
a title card because a default put them there. Add screens when you want them,
in the order you want them.

**If you show a load screen, an empty save list has to say it is empty.**

> **WATCH OUT**
> The save menu takes its slots from a provider that can legitimately return an
> empty list, and an empty list has to render as "no saves". Not as a blank
> panel. A blank panel and a screen that failed to draw look identical, so the
> player cannot tell "I have no saves" from "this game is broken", and they will
> assume the second one.
>
> This is written into the engine header as a scar from a shipped game that had
> twenty save slots, three tiers, corruption detection, a full script API, and no
> screen anywhere that could list a single one of them.

> **TRY IT**
> Add a pause menu with three items: Resume, Options, Quit. Play, pause, resume.
> Then pause and quit. A pause menu you have not quit out of is a pause menu you
> have half tested.

## 6. Putting it on the internet

There is no separate web export command, and no command line.

It is **Build Game**, then set **Platform** to **Web**.

The build gives you a folder that a browser can serve. That is the entire
distance between the editor and a link, and keeping it that short is on purpose:
the engine's design rule is that a person working alone, offline, with no model
in the loop, reaches every capability through the editor interface. Shipping to a
browser is a capability, so it is in a menu.

While you are in that dialog you will see packaging modes. `Packed` is the
default and the right answer. `PackedOpen` leaves assets readable, which is what
you want if you would like people to take your game apart. `LooseFiles` copies
everything raw.

## 7. Somebody taps it on a phone

Touch controls are not a port. You do not build a second control scheme.

The scheme is built from a **fingerprint**: the controller preset your scene
uses, plus every `ActionTriggerComponent` in the scene that asked for a button,
plus anything the project overrides. Change any of those and the scheme rebuilds.

Two things fall out of that, and both are the good kind of surprise:

- **A scene only shows controls it actually has.** The touch buttons and the
  controls hint in the corner come from the same preset, so a level with no jump
  does not show a jump button.
- **Dropping in a component adds its button immediately.** Put an
  `ActionTriggerComponent` in the scene and its touch button is there. You did
  not open a touch layout editor, because there was nothing to lay out.

> **TRY IT**
> You do not need a phone to check this. **View**, then **Simulate Touch
> Controls** turns your mouse into a single finger, in the editor, on any
> platform. `EnjinPlayer --touch` does the same for a built game. Test with one
> finger before you send the link, because everything is easy with a mouse and a
> keyboard and that is not what your friend has.

> **WATCH OUT**
> One finger is one finger. A control scheme that needs two things held at once
> works fine on your keyboard and is impossible on a phone. This is the most
> common way a game that works becomes a game that does not, and simulating touch
> is how you find it before somebody else does.

## Where they end up

A place with weather in it, two ledges and a crossing, a menu and a pause menu
that both quit properly, and a link that opens on a phone and is playable with
one thumb. No command line, no settings window they did not choose to open, and
no scripting.

They also learned, by hitting it rather than by being told, that a configured
component doing nothing is a question about which system reads it. That one is
going to save them more time than the level took.
