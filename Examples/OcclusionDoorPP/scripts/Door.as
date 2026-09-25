// The wall leaves the world in ONE frame, half a second in. Everything behind it
// was hidden last frame and is visible this frame, which is exactly the case
// single-phase occlusion gets wrong: its test reads last frame's depth, which
// still has the wall in it. Occlusion phase 1 re-tests against this frame's.
class Door : TegeBehavior {
    float t = 0.0f;
    void OnUpdate(float dt) {
        t += dt;
        if (t > 0.5f) SetPosition(Vector3(0.0f, -200.0f, 4.0f));
    }
}
