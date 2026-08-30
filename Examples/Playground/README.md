# TEGE Playground

The master showcase scene - one plaza with every major system live:

- NW waterfall (F1 authored motion: UV scroll + foam flipbook + mist particles)
- N lake with buoyant crates (WaterVolume + dynamic rigidbodies)
- NE grove: trees, shrubs, grass tufts swaying in the weather wind
- CW cloth court: flag (edge-pinned), TEARABLE purple drape (run through it),
  tablecloth draped over a table (collide + friction)
- C fire pit (elemental fire) with water dripping into it = steam reactivity,
  plus a classic particle fountain
- CE: CLIMBABLE rope (walk into it and push W - a ladder volume rides the rope),
  lanterns swinging on chains, a clothesline with tearable laundry
- S render-style gallery: matcap, scrolling reflection, flat shading, PS1
  affine+vertex-snap, stipple transparency, UV-scroll conveyor, and a
  reflective wet patch
- Weather EVOLVES on a timer (clear -> rain -> snow -> clear), eased, with
  subtitles + screen-reader announcements; C cycles colorblind modes

Drop a rigged .glb at the marked station for the animated-character exhibit.
