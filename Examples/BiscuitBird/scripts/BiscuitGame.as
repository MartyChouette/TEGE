// Biscuit Bird - the run itself.
//
// Attached to the "Director" entity. Owns everything except how the bird
// flies: the crowd walking the loop, the biscuits they drop, what the bird is
// carrying, the score, the clock and the HUD.
//
// The crowd and the biscuits are pools of entities that already exist in the
// scene (Person0..N, Biscuit0..N) - nothing is spawned at runtime, so a run
// never allocates and the whole cast is visible in the hierarchy.

class BiscuitGame : TegeBehavior {

    // ---------------------------------------------------------------- tuning
    [Property] float runTime      = 120.0f;
    [Property] int   personCount  = 10;
    [Property] int   biscuitCount = 48;
    [Property] int   carryMax     = 4;
    [Property] float ring         = 19.0f;   // half-width of the walkway loop
    [Property] float walkSpeed    = 1.35f;   // an ordinary walking pace, m/s
    [Property] float scareRadius  = 1.7f;
    [Property] float scareSpeed   = 7.0f;    // how fast a dive has to be
    [Property] float pickupRadius = 1.1f;
    [Property] float nestRadius   = 2.0f;
    [Property] float biscuitLife  = 26.0f;

    // --------------------------------------------------------------- handles
    uint64 hud = 0, bird = 0;
    Vector3 nestPos, birdPos, lastBirdPos;
    float birdSpeed = 0.0f;

    // ----------------------------------------------------------- crowd state
    array<uint64> people;
    array<float>  walkT;      // distance along the loop perimeter
    array<float>  lane;       // sideways offset from the path centre line
    array<float>  pace;
    array<float>  stun;       // >0 = startled and standing still
    array<float>  cool;       // >0 = cannot be startled again yet

    // -------------------------------------------------------- biscuit state
    array<uint64> biscuits;
    array<Vector3> bPos;
    array<Vector3> bVel;
    array<float>  bLife;
    array<float>  bSpin;
    array<int>    bState;     // 0 = unused, 1 = tumbling, 2 = resting
    array<uint64> carrySlot;
    int nextBiscuit = 0;

    // ------------------------------------------------------------ run state
    int   phase = 0;          // 0 title, 1 playing, 2 over
    int   score = 0;
    int   carry = 0;
    int   delivered = 0;
    float clock = 0.0f;
    float toastTime = 0.0f;
    float overGrace = 0.0f;
    float now = 0.0f;         // local clock: Time_GetTime() reads 0 in every runtime

    float perim = 0.0f;
    float side  = 0.0f;

    // ------------------------------------------------------------- lifecycle

    void OnStart() {
        hud  = Scene_FindEntity("HUD");
        bird = Scene_FindEntity("Bird");
        uint64 bowl = Scene_FindEntity("NestBowl");
        if (bowl != 0) nestPos = Entity_GetPosition(bowl);
        if (bird != 0) { birdPos = Entity_GetPosition(bird); lastBirdPos = birdPos; }

        side  = ring * 2.0f;
        perim = side * 4.0f;

        for (int i = 0; i < personCount; i++) {
            uint64 e = Scene_FindEntity("Person" + i);
            if (e == 0) break;
            people.insertLast(e);
            walkT.insertLast(perim * (float(i) / float(personCount))
                             + RandomRange(-3.0f, 3.0f));
            lane.insertLast(RandomRange(-1.2f, 1.2f));
            pace.insertLast(RandomRange(0.78f, 1.24f));
            stun.insertLast(0.0f);
            cool.insertLast(0.0f);
        }

        for (int i = 0; i < biscuitCount; i++) {
            uint64 e = Scene_FindEntity("Biscuit" + i);
            if (e == 0) break;
            biscuits.insertLast(e);
            bPos.insertLast(Vector3(0.0f, -8.0f, 0.0f));
            bVel.insertLast(Vector3(0.0f, 0.0f, 0.0f));
            bLife.insertLast(0.0f);
            bSpin.insertLast(RandomRange(0.0f, 360.0f));
            bState.insertLast(0);
            Entity_SetVisible(e, false);
        }

        for (int i = 0; i < carryMax; i++) {
            uint64 e = Scene_FindEntity("Carry" + i);
            if (e == 0) break;
            carrySlot.insertLast(e);
            Entity_SetVisible(e, false);
        }

        if (people.length() == 0)
            Debug_LogWarning("BiscuitGame: no Person entities found");

        clock = runTime;
        ShowTitle();
        RefreshHUD();
    }

    // ------------------------------------------------------------------ tick

    void OnUpdate(float dt) {
        if (dt <= 0.0f) return;
        if (dt > 0.1f) dt = 0.1f;
        now += dt;

        TrackBird(dt);

        if (phase == 0) {
            WalkCrowd(dt);
            if (Tapped()) StartRun();
        } else if (phase == 1) {
            clock -= dt;
            WalkCrowd(dt);
            CheckScares();
            UpdateBiscuits(dt);
            CheckNest();
            if (clock <= 0.0f) { clock = 0.0f; EndRun(); }
        } else {
            overGrace -= dt;
            WalkCrowd(dt);
            UpdateBiscuits(dt);
            if (overGrace <= 0.0f && Tapped()) { Reset(); StartRun(); }
        }

        if (toastTime > 0.0f) {
            toastTime -= dt;
            if (toastTime <= 0.0f) UI_SetText(hud, 7, "");
        }
        RefreshHUD();
    }

    bool Tapped() {
        return Input_GetMouseButtonDown(MouseBtn::Left)
            || Input_GetKeyDown(Key::Space)
            || Input_GetKeyDown(Key::W)
            || Input_GetKeyDown(Key::Up);
    }

    void TrackBird(float dt) {
        if (bird == 0) return;
        lastBirdPos = birdPos;
        birdPos = Entity_GetPosition(bird);
        float dx = birdPos.x - lastBirdPos.x;
        float dy = birdPos.y - lastBirdPos.y;
        float dz = birdPos.z - lastBirdPos.z;
        birdSpeed = Sqrt(dx * dx + dy * dy + dz * dz) / dt;
    }

    // ------------------------------------------------------------ run phases

    void StartRun() {
        phase = 1;
        SendPhase(1);
        UI_SetElementVisible(hud, 4, false);
        UI_SetElementVisible(hud, 5, false);
        Toast("GO", 1.4f);
    }

    void EndRun() {
        phase = 2;
        overGrace = 1.3f;
        SendPhase(2);
        UI_SetElementVisible(hud, 4, true);
        UI_SetElementVisible(hud, 5, true);
        UI_SetText(hud, 4, "TIME - " + score + " POINTS");
        string tail = " biscuits home";
        if (delivered == 1) tail = " biscuit home";
        UI_SetText(hud, 5, "You carried " + delivered + tail + " - TAP to fly again");
        UI_SetText(hud, 7, "");
    }

    void ShowTitle() {
        phase = 0;
        SendPhase(0);
        UI_SetElementVisible(hud, 4, true);
        UI_SetElementVisible(hud, 5, true);
        UI_SetText(hud, 4, "BISCUIT BIRD");
        UI_SetText(hud, 5, "TAP or press SPACE to leave the nest");
    }

    void Reset() {
        score = 0;
        delivered = 0;
        carry = 0;
        clock = runTime;
        UI_SetText(hud, 7, "");
        for (uint i = 0; i < people.length(); i++) {
            walkT[i] = perim * (float(i) / float(people.length()));
            stun[i] = 0.0f;
            cool[i] = 0.0f;
            Entity_SetScale(people[i], Vector3(1.0f, 1.0f, 1.0f));
        }
        for (uint i = 0; i < biscuits.length(); i++) FreeBiscuit(i);
        for (uint i = 0; i < carrySlot.length(); i++)
            Entity_SetVisible(carrySlot[i], false);
    }

    void SendPhase(int p) {
        EventData@ d = EventData();
        d.SetInt("phase", p);
        Events_Send("bb_phase", d);
    }

    // ----------------------------------------------------------- the crowd

    void WalkCrowd(float dt) {
        for (uint i = 0; i < people.length(); i++) {
            if (cool[i] > 0.0f) cool[i] -= dt;

            float squash = 1.0f;
            if (stun[i] > 0.0f) {
                stun[i] -= dt;
                // startled: crouch, cover your head, do not walk
                squash = 1.0f - 0.18f * Min(1.0f, stun[i] * 1.4f);
            } else {
                walkT[i] += walkSpeed * pace[i] * dt;
                if (walkT[i] >= perim) walkT[i] -= perim;
                if (walkT[i] < 0.0f)   walkT[i] += perim;
            }

            Vector3 p = LoopPoint(walkT[i], lane[i]);
            Vector3 ahead = LoopPoint(walkT[i] + 0.6f, lane[i]);
            float bob = 0.0f;
            if (stun[i] <= 0.0f) bob = Abs(Sin(walkT[i] * 2.6f)) * 0.07f;
            p.y = bob;

            Entity_SetPosition(people[i], p);
            Entity_SetRotation(people[i],
                Vector3(0.0f, Degrees(Atan2(-(ahead.x - p.x), -(ahead.z - p.z))), 0.0f));
            Entity_SetScale(people[i],
                Vector3(2.0f - squash, squash, 2.0f - squash));
        }
    }

    // Walk the square loop as one perimeter parameter. Positive lane pushes a
    // pedestrian toward the outside of the block.
    Vector3 LoopPoint(float t, float off) {
        float p = t;
        while (p >= perim) p -= perim;
        while (p < 0.0f)   p += perim;

        if (p < side)
            return Vector3(-ring + p, 0.0f, -ring - off);
        if (p < side * 2.0f)
            return Vector3(ring + off, 0.0f, -ring + (p - side));
        if (p < side * 3.0f)
            return Vector3(ring - (p - side * 2.0f), 0.0f, ring + off);
        return Vector3(-ring - off, 0.0f, ring - (p - side * 3.0f));
    }

    void CheckScares() {
        if (birdSpeed < scareSpeed || birdPos.y > 5.0f) return;
        for (uint i = 0; i < people.length(); i++) {
            if (stun[i] > 0.0f || cool[i] > 0.0f) continue;
            Vector3 p = Entity_GetPosition(people[i]);
            float dx = birdPos.x - p.x;
            float dy = birdPos.y - (p.y + 1.05f);
            float dz = birdPos.z - p.z;
            if (dx * dx + dy * dy + dz * dz > scareRadius * scareRadius) continue;
            Startle(i, p);
        }
    }

    void Startle(uint i, Vector3 p) {
        stun[i] = 2.1f;
        cool[i] = 6.5f;
        Audio_PlayAtPosition("assets/yelp.wav", p);

        int drops = 2;
        if (birdSpeed > 13.0f) drops = 3;
        if (birdSpeed > 18.0f) drops = 4;
        for (int d = 0; d < drops; d++) {
            float a = RandomRange(0.0f, 6.2832f);
            float r = RandomRange(1.1f, 2.4f);
            Drop(Vector3(p.x, p.y + 1.1f, p.z),
                 Vector3(Cos(a) * r, RandomRange(2.6f, 4.2f), Sin(a) * r));
        }
        Toast("BISCUITS!", 1.1f);
    }

    // ---------------------------------------------------------- the biscuits

    void Drop(Vector3 at, Vector3 vel) {
        if (biscuits.length() == 0) return;
        // Round-robin the pool; the oldest resting biscuit gets recycled.
        for (uint tries = 0; tries < biscuits.length(); tries++) {
            uint i = uint(nextBiscuit) % biscuits.length();
            nextBiscuit = int(i) + 1;
            if (bState[i] != 0) continue;
            bPos[i] = at;
            bVel[i] = vel;
            bLife[i] = biscuitLife;
            bState[i] = 1;
            Entity_SetVisible(biscuits[i], true);
            Entity_SetPosition(biscuits[i], at);
            return;
        }
        // Pool full: steal the slot with the least life left.
        uint worst = 0;
        for (uint i = 1; i < biscuits.length(); i++)
            if (bLife[i] < bLife[worst]) worst = i;
        bPos[worst] = at;
        bVel[worst] = vel;
        bLife[worst] = biscuitLife;
        bState[worst] = 1;
        Entity_SetVisible(biscuits[worst], true);
    }

    void FreeBiscuit(uint i) {
        bState[i] = 0;
        bLife[i] = 0.0f;
        bPos[i] = Vector3(0.0f, -8.0f, 0.0f);
        Entity_SetVisible(biscuits[i], false);
        Entity_SetPosition(biscuits[i], bPos[i]);
    }

    void UpdateBiscuits(float dt) {
        for (uint i = 0; i < biscuits.length(); i++) {
            if (bState[i] == 0) continue;

            bLife[i] -= dt;
            if (bLife[i] <= 0.0f) { FreeBiscuit(i); continue; }

            if (bState[i] == 1) {
                bVel[i].y -= 17.0f * dt;
                bPos[i].x += bVel[i].x * dt;
                bPos[i].y += bVel[i].y * dt;
                bPos[i].z += bVel[i].z * dt;
                if (bPos[i].y <= 0.06f) {
                    bPos[i].y = 0.06f;
                    if (bVel[i].y < -1.8f) {
                        bVel[i].y = -bVel[i].y * 0.32f;   // one small bounce
                        bVel[i].x *= 0.5f;
                        bVel[i].z *= 0.5f;
                    } else {
                        bState[i] = 2;
                        bVel[i] = Vector3(0.0f, 0.0f, 0.0f);
                    }
                }
                bSpin[i] += dt * 420.0f;
            } else {
                bSpin[i] += dt * 55.0f;
                bPos[i].y = 0.06f + Sin(now * 3.0f + bSpin[i] * 0.02f) * 0.035f;
            }

            // last few seconds: blink so a stale biscuit is not a surprise
            bool show = true;
            if (bLife[i] < 4.0f) show = (int(bLife[i] * 6.0f) % 2) == 0;
            Entity_SetVisible(biscuits[i], show);

            Entity_SetPosition(biscuits[i], bPos[i]);
            Entity_SetRotation(biscuits[i], Vector3(bState[i] == 1 ? bSpin[i] : 0.0f,
                                                    bSpin[i], 0.0f));

            if (phase == 1 && carry < carryMax) TryPickup(i);
        }
    }

    void TryPickup(uint i) {
        float dx = birdPos.x - bPos[i].x;
        float dy = birdPos.y - bPos[i].y;
        float dz = birdPos.z - bPos[i].z;
        if (dx * dx + dy * dy + dz * dz > pickupRadius * pickupRadius) return;

        Audio_PlayAtPosition("assets/pickup.wav", bPos[i]);
        FreeBiscuit(i);
        carry += 1;
        for (uint c = 0; c < carrySlot.length(); c++)
            Entity_SetVisible(carrySlot[c], int(c) < carry);
        if (carry >= carryMax) Toast("BEAK FULL - GET HOME", 1.6f);
    }

    void CheckNest() {
        if (carry <= 0) return;
        float dx = birdPos.x - nestPos.x;
        float dy = birdPos.y - nestPos.y;
        float dz = birdPos.z - nestPos.z;
        if (dx * dx + dy * dy + dz * dz > nestRadius * nestRadius) return;

        int gained = carry * 10;
        if (carry >= carryMax) gained += 20;      // full-beak bonus
        score += gained;
        delivered += carry;
        carry = 0;
        for (uint c = 0; c < carrySlot.length(); c++)
            Entity_SetVisible(carrySlot[c], false);
        Audio_PlayAtPosition("assets/deposit.wav", nestPos);
        Toast("+" + gained, 1.4f);
    }

    // ------------------------------------------------------------------- HUD

    void Toast(const string &in msg, float secs) {
        UI_SetText(hud, 7, msg);
        toastTime = secs;
    }

    void RefreshHUD() {
        if (hud == 0) return;
        UI_SetText(hud, 1, "BISCUITS  " + score);
        UI_SetText(hud, 2, "CARRYING  " + carry + " / " + carryMax);
        UI_SetText(hud, 3, Clock());
    }

    string Clock() {
        int total = int(clock + 0.999f);
        if (total < 0) total = 0;
        int m = total / 60;
        int s = total % 60;
        if (s < 10) return "" + m + ":0" + s;
        return "" + m + ":" + s;
    }
}
