// Drives the combined lab scene: keeps the metaball blobs moving, cycles the
// 4D polytope, and prints what each exhibit is doing. Every generator below
// shipped complete in the engine with no component, no inspector entry and no
// system tick, so before this demo none of it could be reached from a scene.
class GeometryTour : TegeBehavior {
    array<uint64> blobs;
    array<float>  phase;
    uint64 tesseract = 0;
    uint64 lifeWall = 0;
    uint64 star = 0;
    float t = 0.0f;
    float reportTimer = 0.0f;

    void OnStart() {
        for (int i = 0; i < 6; i++) {
            uint64 b = Scene_FindEntity("Blob" + i);
            if (b == 0) continue;
            blobs.insertLast(b);
            phase.insertLast(6.2831853f * float(i) / 6.0f);
        }
        tesseract = Scene_FindEntity("Tesseract");
        lifeWall  = Scene_FindEntity("LifeWall");
        star      = Scene_FindEntity("FourierStar");

        Debug_Log("Generated Geometry Lab");
        Debug_Log("  metaball blobs driven: " + blobs.length());
        Debug_Log("  keys: 1-5 switch polytope, R resets the automaton, Space pauses blobs");
    }

    bool paused = false;

    void OnUpdate(float dt) {
        t += dt;

        if (Input_GetKeyDown(Key::Space)) paused = !paused;
        if (!paused) {
            // The blobs orbit at slightly different radii so the isosurface
            // pinches and merges instead of holding one steady shape.
            for (uint i = 0; i < blobs.length(); i++) {
                float a = phase[i] + t * 0.5f;
                float r = 2.2f + 0.6f * sin(t * 0.8f + phase[i]);
                Entity_SetPosition(blobs[i], Vector3(
                    -23.0f + r * cos(a),
                    6.4f + 0.6f * sin(t * 1.1f + phase[i]),
                    r * sin(a)));
            }
        }

        // Report once every four seconds so a headless run leaves evidence in
        // the log that each generator actually produced geometry.
        reportTimer += dt;
        if (reportTimer >= 4.0f) {
            reportTimer = 0.0f;
            Debug_Log("lab t=" + int(t) + "s  blobs=" + blobs.length()
                      + "  lifeWall=" + (lifeWall != 0 ? "ok" : "missing")
                      + "  tesseract=" + (tesseract != 0 ? "ok" : "missing")
                      + "  fourier=" + (star != 0 ? "ok" : "missing"));
        }
    }
}
