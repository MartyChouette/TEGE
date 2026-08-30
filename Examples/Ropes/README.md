# Ropes + Chains (G3/G5)

Four setups showing the configurations:

- **SwingRope** - a plain rope hanging from the beam. Weather wind sways it;
  walk into it and it reacts (collide pushes points out of your capsule).
- **LanternRope** - `endAttachName: Lantern` with `endMass 3`: the lantern
  entity is dragged along at the rope tip and its weight pulls the rope taut.
- **HangingChain** - `style: 1` (Chain): the same simulation drawn as rigid
  links with an alternating 90-degree twist. Chains read best with ~8-12
  segments, a bigger thickness, and high iterations (stiff hang).
- **Clothesline** - `endAttachName: LineEnd` with `pinBottom: true`: the tip
  anchors to the LineEnd entity instead of dangling, so the rope spans
  between the posts and sags (its length is longer than the span).

The rope entity needs a MATERIAL for the tube's look. Length, segments,
thickness, and the attach fields rebuild the rope when edited; the tube is
re-simulated every frame (verlet chain, same solver family as cloth).
