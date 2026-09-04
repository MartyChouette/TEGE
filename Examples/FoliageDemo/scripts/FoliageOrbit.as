// Orbit camera for the Foliage demo.
//
// The demo has no character to control, so touch had nothing to do: the generic
// preset put a move stick and a look region on screen for a camera that never
// moved. This gives the same gesture on every input: drag to swing around the
// trees, pinch or wheel to move in and out.
//
// Drag arrives as mouse delta on all platforms. On web a touch in the look
// region already accumulates into the same delta (Core Input touch role 2), so
// one code path covers mouse and finger. Zoom takes the wheel on desktop and
// Input_GetPinchDelta() on touch.
class FoliageOrbit : TegeBehavior {
    uint64 cam = 0;

    // Where we orbit: the middle tree, roughly at eye height of the canopy.
    float centerX = 0.0f, centerY = 3.0f, centerZ = 2.0f;

    float yaw = 0.0f;          // degrees, 0 = looking down -Z
    float pitch = -12.0f;      // degrees, negative looks down
    float dist = 26.0f;

    // Feel. Distances are world units, angles degrees.
    // (AngelScript has no const class properties, so these are plain members.)
    float MIN_DIST = 6.0f;
    float MAX_DIST = 60.0f;
    float MIN_PITCH = -70.0f;
    float MAX_PITCH = 55.0f;
    float DRAG_SPEED = 0.25f;    // degrees per pixel
    float PINCH_SPEED = 0.05f;   // world units per pixel of finger spread
    float WHEEL_SPEED = 2.0f;    // world units per wheel notch
    float IDLE_SPIN = 3.0f;      // degrees/sec when nobody is touching it

    float idleFor = 0.0f;

    void OnStart() {
        cam = Scene_FindEntity("MainCam");
        if (cam != 0) Camera_TakeManualControl(cam);

        // Full-screen drag, no stick, no buttons: every gesture is camera.
        Touch_ClearButtons();
        Touch_SetStick(false, 0, 0, 0, 0);
        Touch_SetLookRegion(true);

        Subtitle_Show("Drag to orbit - pinch or scroll to zoom", "", 3.5f);
        Apply();
    }

    void OnUpdate(float dt) {
        if (cam == 0) return;

        bool touched = false;

        // ---- orbit -------------------------------------------------------
        // Only while a pointer is down, so the camera does not chase a hovering
        // mouse. A two-finger pinch must not also spin the view, or zooming
        // yanks the camera sideways.
        if (Input_GetMouseButton(0) && Input_GetTouchCount() < 2) {
            Vector2 d = Input_GetMouseDelta();
            if (d.x != 0.0f || d.y != 0.0f) {
                yaw -= d.x * DRAG_SPEED;
                pitch -= d.y * DRAG_SPEED;
                touched = true;
            }
        }

        // ---- zoom --------------------------------------------------------
        float pinch = Input_GetPinchDelta();
        if (pinch != 0.0f) { dist -= pinch * PINCH_SPEED; touched = true; }

        Vector2 wheel = Input_GetScrollDelta();
        if (wheel.y != 0.0f) { dist -= wheel.y * WHEEL_SPEED; touched = true; }

        // ---- idle drift --------------------------------------------------
        // Left alone the demo should still look alive, but it must not fight
        // the player: only after a couple of seconds of no input.
        if (touched) idleFor = 0.0f; else idleFor += dt;
        if (idleFor > 2.0f) yaw += IDLE_SPIN * dt;

        if (pitch < MIN_PITCH) pitch = MIN_PITCH;
        if (pitch > MAX_PITCH) pitch = MAX_PITCH;
        if (dist < MIN_DIST) dist = MIN_DIST;
        if (dist > MAX_DIST) dist = MAX_DIST;

        Apply();
    }

    void Apply() {
        float ry = yaw * 0.017453292f;
        float rp = pitch * 0.017453292f;
        float cp = cos(rp);

        // Spherical offset from the orbit centre. The camera then LOOKS at the
        // centre, which is just the orbit angles negated back.
        float ox = sin(ry) * cp * dist;
        float oy = -sin(rp) * dist;
        float oz = cos(ry) * cp * dist;

        Entity_SetPosition(cam, Vector3(centerX + ox, centerY + oy, centerZ + oz));
        Entity_SetRotation(cam, Vector3(pitch, yaw, 0.0f));
    }
}
