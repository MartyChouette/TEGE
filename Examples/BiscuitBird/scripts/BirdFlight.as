// Biscuit Bird - flight, wings and chase camera.
//
// Attached to the "BirdBrain" entity. Owns the bird's transform completely:
// the flight model is script kinematics, not physics, so the feel is the same
// on every machine and nothing can knock the bird out of the air.
//
// One button. A tap flaps. Holding on the left or right of the screen (or
// A/D, or the arrow keys) banks that way. Gravity does the rest.
//
// Phase is owned by BiscuitGame and arrives on the "bb_phase" event:
//   0 = perched on the nest (title), 1 = flying, 2 = run over.

class BirdFlight : TegeBehavior {

    // ---------------------------------------------------------------- tuning
    [Property] float gravity     = 11.5f;   // constant downward pull
    [Property] float flapImpulse = 7.4f;    // upward kick per flap
    [Property] float cruiseSpeed = 13.0f;   // level forward speed
    [Property] float diveSpeed   = 24.0f;   // cap when stooping
    [Property] float turnRate    = 118.0f;  // degrees/sec at full bank
    [Property] float maxHeight   = 46.0f;
    [Property] float parkRadius  = 46.0f;   // soft wall around the park
    [Property] float camDistance = 4.6f;
    [Property] float camHeight   = 1.55f;

    // --------------------------------------------------------------- handles
    uint64 bird = 0, cam = 0, shadow = 0, wingL = 0, wingR = 0;

    // ----------------------------------------------------------------- state
    Vector3 pos, perch, camPos;
    float yaw = 0.0f, vy = 0.0f, speed = 0.0f;
    float steer = 0.0f, steerVis = 0.0f;
    float flapPhase = 99.0f;
    float roll = 0.0f, pitch = 0.0f;
    float landTimer = 0.0f;
    float now = 0.0f;        // local clock: Time_GetTime() reads 0 in every runtime
    int   phase = 0;
    bool  camReady = false;

    // ------------------------------------------------------------- lifecycle

    void OnStart() {
        bird   = Scene_FindEntity("Bird");
        cam    = Scene_FindEntity("MainCam");
        shadow = Scene_FindEntity("BirdShadow");
        wingL  = Scene_FindEntity("BirdWingL");
        wingR  = Scene_FindEntity("BirdWingR");

        if (bird == 0) {
            Debug_LogError("BirdFlight: no entity named Bird in the scene");
            return;
        }
        perch  = Entity_GetPosition(bird);
        pos    = perch;
        camPos = Vector3(perch.x, perch.y + camHeight, perch.z + camDistance);

        Events_Listen("bb_phase", EventCallback(this.OnPhase));
    }

    void OnPhase(const string &in name) {
        int p = Events_CurrentInt("phase");
        if (p == phase) return;
        int was = phase;
        phase = p;
        if (p == 0) Perch();
        if (p == 2) landTimer = 1.4f;
        // Restarting straight out of a finished run: put the bird back on the
        // nest and launch it, so a retry starts from the same place as run one.
        if (p == 1 && was == 2) { Perch(); Flap(); }
    }

    void Perch() {
        pos = perch;
        vy = 0.0f; speed = 0.0f; yaw = 0.0f;
        steer = 0.0f; steerVis = 0.0f; roll = 0.0f; pitch = 0.0f;
        flapPhase = 99.0f;
    }

    // ------------------------------------------------------------------ tick

    void OnUpdate(float dt) {
        if (bird == 0 || dt <= 0.0f) return;
        if (dt > 0.1f) dt = 0.1f;           // never let a hitch teleport the bird
        now += dt;

        bool tapped = Tapped();

        if (phase == 0) {
            // Perched. The tap that starts the run is also the first flap, so
            // leaving the nest feels like one motion.
            Idle(dt);
            if (tapped) { phase = 1; Flap(); }
        } else if (phase == 1) {
            Fly(dt, tapped);
        } else {
            ReturnHome(dt);
        }

        Apply(dt);
    }

    // ----------------------------------------------------------------- input

    bool Tapped() {
        return Input_GetMouseButtonDown(MouseBtn::Left)
            || Input_GetKeyDown(Key::Space)
            || Input_GetKeyDown(Key::W)
            || Input_GetKeyDown(Key::Up);
    }

    float SteerInput() {
        float s = 0.0f;
        if (Input_GetKey(Key::A) || Input_GetKey(Key::Left))  s -= 1.0f;
        if (Input_GetKey(Key::D) || Input_GetKey(Key::Right)) s += 1.0f;

        // Held pointer: distance from the middle of the screen is the stick.
        if (Input_GetMouseButton(MouseBtn::Left)) {
            Vector2 size = Input_GetScreenSize();
            if (size.x > 1.0f) {
                Vector2 m = Input_GetMousePosition();
                float nx = (m.x / size.x - 0.5f) * 2.6f;
                if (Abs(nx) > 0.14f) s += Clamp(nx, -1.0f, 1.0f);
            }
        }
        return Clamp(s, -1.0f, 1.0f);
    }

    // ---------------------------------------------------------------- flight

    void Flap() {
        // Clamping first means a flap out of a dive is a real recovery, but
        // mashing while already climbing does not stack into orbit.
        vy = Min(vy, 2.0f) + flapImpulse;
        vy = Min(vy, 11.0f);
        flapPhase = 0.0f;
        speed = Max(speed, 7.0f);
        Audio_PlayAtPosition("assets/flap.wav", pos);
    }

    void Fly(float dt, bool tapped) {
        if (tapped) Flap();

        float want = SteerInput();

        // Soft wall: out past the treeline the bird leans back toward the park
        // instead of hitting an invisible fence.
        float distSq = pos.x * pos.x + pos.z * pos.z;
        if (distSq > parkRadius * parkRadius) {
            float homeYaw = Degrees(Atan2(pos.x, pos.z));
            float diff = WrapAngle(homeYaw - yaw);
            want = Clamp(want - Sign(diff) * 0.9f, -1.0f, 1.0f);
        }

        steer += (want - steer) * Min(1.0f, dt * 9.0f);
        yaw -= steer * turnRate * dt;

        vy -= gravity * dt;
        vy = Max(vy, -27.0f);

        // Diving trades height for speed; climbing bleeds it off.
        float target = Clamp(cruiseSpeed - vy * 0.55f, 6.5f, diveSpeed);
        speed += (target - speed) * Min(1.0f, dt * 1.7f);

        float fx = -Sin(Radians(yaw));
        float fz = -Cos(Radians(yaw));
        pos.x += fx * speed * dt;
        pos.z += fz * speed * dt;
        pos.y += vy * dt;

        if (pos.y < 0.55f) {                // skidding along the grass is fine
            pos.y = 0.55f;
            vy = Max(vy, 0.0f);
            speed *= 0.985f;
        }
        if (pos.y > maxHeight) { pos.y = maxHeight; vy = Min(vy, 0.0f); }

        pos.x = Clamp(pos.x, -62.0f, 62.0f);
        pos.z = Clamp(pos.z, -62.0f, 62.0f);
    }

    void Idle(float dt) {
        pos.x = perch.x;
        pos.z = perch.z;
        pos.y = perch.y + Sin(now * 1.6f) * 0.06f;
        yaw += dt * 9.0f;                   // slow look around from the nest
        speed = 0.0f; vy = 0.0f;
    }

    void ReturnHome(float dt) {
        // Run over: the bird coasts back to the nest and settles.
        landTimer -= dt;
        float k = Min(1.0f, dt * 1.6f);
        pos.x += (perch.x - pos.x) * k;
        pos.y += (perch.y - pos.y) * k;
        pos.z += (perch.z - pos.z) * k;
        steer *= 0.9f;
        speed *= 0.94f;
        vy = 0.0f;
        if (landTimer <= 0.0f) flapPhase = 99.0f;
    }

    // ------------------------------------------------------- transform + cam

    void Apply(float dt) {
        steerVis += (steer - steerVis) * Min(1.0f, dt * 7.0f);

        float wantPitch = Clamp(vy * 2.3f, -52.0f, 34.0f);
        pitch += (wantPitch - pitch) * Min(1.0f, dt * 8.0f);
        roll   = -steerVis * 36.0f;

        Entity_SetPosition(bird, pos);
        Entity_SetRotation(bird, Vector3(pitch, yaw, roll));

        // wings
        float wing;
        if (flapPhase < 6.2832f) {
            flapPhase += dt * 13.5f;
            wing = Sin(flapPhase) * 52.0f;
        } else {
            wing = Sin(now * 2.2f) * 6.0f - 3.0f;
        }
        if (wingL != 0) Entity_SetRotation(wingL, Vector3(0.0f, 0.0f, wing));
        if (wingR != 0) Entity_SetRotation(wingR, Vector3(0.0f, 0.0f, -wing));

        // ground shadow - the only depth cue that matters when you are diving
        if (shadow != 0) {
            float s = Clamp(1.9f - pos.y * 0.03f, 0.5f, 1.9f);
            Entity_SetPosition(shadow, Vector3(pos.x, 0.09f, pos.z));
            Entity_SetScale(shadow, Vector3(s, 0.02f, s));
        }

        UpdateCamera(dt);
    }

    void UpdateCamera(float dt) {
        if (cam == 0) return;

        float fx = -Sin(Radians(yaw));
        float fz = -Cos(Radians(yaw));

        Vector3 want;
        want.x = pos.x - fx * camDistance;
        want.y = pos.y + camHeight;
        want.z = pos.z - fz * camDistance;
        if (want.y < 1.2f) want.y = 1.2f;

        if (!camReady) { camPos = want; camReady = true; }
        float rate = 2.2f;
        if (phase == 1) rate = 5.5f;
        float k = Min(1.0f, dt * rate);
        camPos.x += (want.x - camPos.x) * k;
        camPos.y += (want.y - camPos.y) * k;
        camPos.z += (want.z - camPos.z) * k;

        float dx = (pos.x + fx * 4.0f) - camPos.x;
        float dy = (pos.y + 0.35f) - camPos.y;
        float dz = (pos.z + fz * 4.0f) - camPos.z;
        float flat = Sqrt(dx * dx + dz * dz);
        if (flat < 0.001f) flat = 0.001f;

        Entity_SetPosition(cam, camPos);
        Entity_SetRotation(cam, Vector3(Degrees(Atan2(dy, flat)),
                                        Degrees(Atan2(-dx, -dz)),
                                        roll * 0.22f));
    }

    float WrapAngle(float a) {
        while (a > 180.0f)  a -= 360.0f;
        while (a < -180.0f) a += 360.0f;
        return a;
    }
}
