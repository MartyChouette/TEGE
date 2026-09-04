// Moves the metaball blobs so the isosurface visibly merges and separates.
// The generator itself is CPU-side marching cubes over whatever positions the
// blobs happen to be at, so nothing here touches the mesh: it just moves
// entities and the surface follows on the next rebuild.
class BlobOrbit : TegeBehavior {
    array<uint64> blobs;
    array<float>  phase;
    array<float>  radius;
    float t = 0.0f;
    bool paused = false;

    void OnStart() {
        for (int i = 0; i < 7; i++) {
            uint64 b = Scene_FindEntity("Blob" + i);
            if (b == 0) continue;
            blobs.insertLast(b);
            phase.insertLast(6.2831853f * float(i) / 7.0f);
            // Alternating radii make the blobs pass through each other rather
            // than orbiting in a rigid ring, which is what shows the merge.
            radius.insertLast((i % 2 == 0) ? 2.9f : 1.7f);
        }
        Debug_Log("BlobOrbit: driving " + blobs.length() + " metaballs");
    }

    void OnUpdate(float dt) {
        if (Input_GetKeyDown(Key::Space)) paused = !paused;
        if (paused) return;

        t += dt;
        for (uint i = 0; i < blobs.length(); i++) {
            float a = phase[i] + t * 0.55f;
            float r = radius[i] + 0.5f * sin(t * 0.9f + phase[i]);
            Entity_SetPosition(blobs[i], Vector3(
                r * cos(a),
                1.4f + 0.7f * sin(t * 1.3f + phase[i]),
                r * sin(a)));
        }
    }
}
