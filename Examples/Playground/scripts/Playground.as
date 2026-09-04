// Evolving weather + accessibility for the TEGE Playground.
class Playground : TegeBehavior {
    float t = 0.0f;
    int phase = -1;          // -1 forces the first announce
    int cbMode = 0;
    bool bullet = false;
    float rain = 0.0f, snow = 0.0f;      // current, eased toward targets
    float rainT = 0.0f, snowT = 0.0f;    // targets per phase

    void OnStart() {
        Subtitle_Show("Welcome to the TEGE Playground", "", 4.0f);
        Announcer_Announce("Playground loaded. Weather will evolve on its own.");

        // Bullet time is a game action, not a hardcoded key: naming a Custom
        // slot lists it in the controls menu + hint, and the touch button
        // presses whatever it is bound to (rebinds included).
        InputAction_SetName(GameAction::Custom0, "SLO-MO");
        InputAction_Rebind(GameAction::Custom0, Key::B);
        InputAction_AddGamepadBinding(GameAction::Custom0, GamepadBtn::Y);
        Touch_AddActionButton("SLO-MO", GameAction::Custom0, 0, 2, 0.115f);
    }

    void OnUpdate(float dt) {
        // ---- evolving weather: 18s phases, intensities EASE between them ----
        t += dt;
        int p = int(t / 18.0f) % 4;
        if (p != phase) {
            phase = p;
            // Weather_Set drives the TYPE (the sim lerps intensities toward
            // the type's profile - setting intensity alone fights Clear).
            if (p == 0) { rainT = 0.0f; snowT = 0.0f; Weather_Set(0, 3.0f); Subtitle_Show("Weather: clear skies", "", 2.5f); }
            if (p == 1) { rainT = 0.85f; snowT = 0.0f; Weather_Set(2, 3.0f); Subtitle_Show("Weather: rain rolling in", "", 2.5f); Render_SetRainActive(true); }
            if (p == 2) { rainT = 0.0f; snowT = 0.8f; Weather_Set(4, 3.0f); Subtitle_Show("Weather: turning to snow", "", 2.5f); }
            if (p == 3) { rainT = 0.0f; snowT = 0.0f; Weather_Set(0, 3.0f); Subtitle_Show("Weather: clearing up", "", 2.5f); }
        }
        rain += (rainT - rain) * min(dt * 0.6f, 1.0f);
        snow += (snowT - snow) * min(dt * 0.6f, 1.0f);
        Weather_SetRainIntensity(rain);
        Weather_SetSnowIntensity(snow);
        if (rain < 0.02f && rainT == 0.0f) Render_SetRainActive(false);

        // ---- bullet time (Custom0: B / gamepad Y / touch SLO-MO) ----
        if (InputAction_IsPressed(GameAction::Custom0)) {
            bullet = !bullet;
            uint64 me = Scene_FindEntity("Player");
            Time_SetScale(bullet ? 0.25f : 1.0f);
            if (me != 0) Controller_SetIgnoreTimeScale(me, bullet);
            Subtitle_Show(bullet ? "BULLET TIME - the world slows, you don't"
                                 : "Bullet time off", "", 2.5f);
        }

        // ---- accessibility: C cycles colorblind simulation modes ----
        if (Input_GetKeyDown(Key::C)) {
            cbMode = (cbMode + 1) % 4;
            Colorblind_SetMode(cbMode);
            string name = "off";
            if (cbMode == 1) name = "protanopia";
            if (cbMode == 2) name = "deuteranopia";
            if (cbMode == 3) name = "tritanopia";
            Subtitle_Show("Colorblind mode: " + name, "", 2.0f);
            Announcer_Announce("Colorblind mode " + name);
        }
    }

    float min(float a, float b) { return a < b ? a : b; }
}
