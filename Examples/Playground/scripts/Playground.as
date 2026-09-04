// Accessibility, bullet time, and hands-on control of the sky for the TEGE
// Playground.
//
// Weather is chosen by the SEASON: the scene turns on world time and seasonal
// weather, so SeasonalWeatherSystem picks a type from the season's
// probabilities and the temperature. The buttons below let you push it around
// without taking that away -- WEATHER forces the next type immediately, SEASON
// jumps the calendar, and the season reclaims the weather at its next
// transition. That is the rule the engine enforces, shown rather than
// described.
class Playground : TegeBehavior {
    int cbMode = 0;
    bool bullet = false;
    int weatherIdx = 0;
    string lastSeason = "";

    // Weather types, in the order the button cycles them.
    // 0 Clear, 1 Cloudy, 2 Rain, 3 HeavyRain, 4 Snow, 5 Fog, 6 Storm
    array<int> weathers = { 0, 2, 3, 4, 5, 6 };
    array<string> weatherNames = { "Clear", "Rain", "Heavy rain", "Snow", "Fog", "Storm" };

    void OnStart() {
        Subtitle_Show("Welcome to the TEGE Playground", "", 4.0f);
        Announcer_Announce("Playground loaded. Weather follows the season. Press B for slow motion, V for weather, N for the season.");

        // Every one of these is a named game ACTION, not a hardcoded key: naming
        // a Custom slot lists it in the controls menu and the on-screen hint,
        // and the touch button presses whatever it is bound to, rebinds
        // included. A key that is only readable through Input_GetKeyDown can
        // never be rebound, never appears in the hint, and never reaches a
        // touch device.
        InputAction_SetName(GameAction::Custom0, "SLO-MO");
        InputAction_Rebind(GameAction::Custom0, Key::B);
        InputAction_AddGamepadBinding(GameAction::Custom0, GamepadBtn::Y);
        Touch_AddActionButton("SLO-MO", GameAction::Custom0, 0, 2, 0.115f);

        InputAction_SetName(GameAction::Custom1, "WEATHER");
        InputAction_Rebind(GameAction::Custom1, Key::V);
        InputAction_AddGamepadBinding(GameAction::Custom1, GamepadBtn::X);
        Touch_AddActionButton("WEATHER", GameAction::Custom1, 1, 2, 0.115f);

        InputAction_SetName(GameAction::Custom2, "SEASON");
        InputAction_Rebind(GameAction::Custom2, Key::N);
        InputAction_AddGamepadBinding(GameAction::Custom2, GamepadBtn::B);
        Touch_AddActionButton("SEASON", GameAction::Custom2, 2, 2, 0.115f);

        lastSeason = WorldTime_GetSeasonName();
    }

    void OnUpdate(float dt) {
        // The rain-ripple flag follows whatever the weather actually is, from
        // whichever source set it.
        Render_SetRainActive(Weather_GetRainIntensity() > 0.02f);

        // ---- bullet time (Custom0) ----
        if (InputAction_IsPressed(GameAction::Custom0)) {
            bullet = !bullet;
            uint64 me = Scene_FindEntity("Player");
            Time_SetScale(bullet ? 0.25f : 1.0f);
            if (me != 0) Controller_SetIgnoreTimeScale(me, bullet);
            Subtitle_Show(bullet ? "BULLET TIME - the world slows, you don't"
                                 : "Bullet time off", "", 2.5f);
        }

        // ---- weather (Custom1): force the next type right now ----
        if (InputAction_IsPressed(GameAction::Custom1)) {
            weatherIdx = (weatherIdx + 1) % weathers.length();
            int type = weathers[weatherIdx];
            Weather_Set(type, 1.5f);
            // The type drives the sim's own intensities; nudging them here just
            // makes the change immediate instead of waiting on the lerp.
            if (type == 2) { Weather_SetRainIntensity(0.7f); Weather_SetSnowIntensity(0.0f); }
            else if (type == 3 || type == 6) { Weather_SetRainIntensity(1.0f); Weather_SetSnowIntensity(0.0f); }
            else if (type == 4) { Weather_SetRainIntensity(0.0f); Weather_SetSnowIntensity(0.8f); }
            else if (type == 0) { Weather_SetRainIntensity(0.0f); Weather_SetSnowIntensity(0.0f); }

            string note = weatherNames[weatherIdx];
            if (WorldTime_GetSeasonalWeather()) {
                note += " (the season takes it back shortly)";
            }
            Subtitle_Show("Weather: " + note, "", 2.5f);
            Announcer_Announce("Weather " + weatherNames[weatherIdx]);
        }

        // ---- season (Custom2): jump the calendar a quarter ----
        if (InputAction_IsPressed(GameAction::Custom2)) {
            WorldTime_AdvanceSeason();
            string s = WorldTime_GetSeasonName();
            lastSeason = s;
            Subtitle_Show("Season: " + s + " - foliage and weather follow", "", 3.0f);
            Announcer_Announce("Season " + s);
        }

        // The clock can roll the season on its own, so announce that too rather
        // than only reacting to the button.
        string now = WorldTime_GetSeasonName();
        if (now != lastSeason) {
            lastSeason = now;
            Subtitle_Show("The season turns: " + now, "", 3.0f);
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
