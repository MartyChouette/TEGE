# Ladders (G1)

A tower with a climbable ladder. Walk into the ladder and push W to climb;
S climbs down, Space hops off, and climbing past the top mantles onto the roof.

The `LadderVolume` entity carries the `ladder` component. Two authoring rules:

- Half extents are WORLD units (entity scale does not multiply them).
- Extend the volume about 1 unit PAST the ledge you want to land on, like a
  real ladder sticking above a roofline. The mantle boost fires near the
  volume's top, so the top has to be at or above the ledge for the character's
  feet to clear it.

Works with both the Third Person and First Person controllers.
