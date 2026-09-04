// Accessibility and bullet time for the TEGE Playground.
//
// Weather is NOT driven from here any more. The scene turns on world time and
// seasonal weather, so the season and the temperature choose it -- which is
// what a scene with seasons should do, rather than every project hand-rolling
// its own cycle.
class Playground : TegeBehavior {
    int cbMode = 0;
    bool bullet = false;

    void OnStart() {
        Subtitle_Show("Welcome to the TEGE Playground", "", 4.0f);
        Announcer_Announce("Playground loaded. Weather follows the season.");

        // Bullet time is a game action, not a hardcoded key: naming a Custom
        // slot lists it in the controls menu + hint, and the touch button
        // presses whatever it is bound to (rebinds included).
        InputAction_SetName(GameAction::Custom0, "SLO-MO");
        InputAction_Rebind(GameAction::Custom0, Key::B);
        InputAction_AddGamepadBinding(GameAction::Custom0, GamepadBtn::Y);
        Touch_AddActionButton("SLO-MO", GameAction::Custom0, 0, 2, 0.115f);
    }

    void OnUpdate(float dt) {
        // Weather comes from the SEASONS now. The scene enables world time and
        // seasonal weather, so SeasonalWeatherSystem picks a type from the
        // season's probabilities and the temperature, and the rain-active flag
        // follows whatever it chose. This used to be a hand-rolled 18-second
        // cycle here, which is the kind of thing a scene should not have to own.
        Render_SetRainActive(Weather_GetRainIntensity() > 0.02f);

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
