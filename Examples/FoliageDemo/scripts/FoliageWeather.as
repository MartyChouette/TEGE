// Evolving weather for the Foliage demo: clear -> windy -> rain -> snow, looping.
// Shows the foliage (procedural + imported) reacting to precipitation while it sways
// in the wind. Precipitation eases between phases so it rolls in/out smoothly.
class FoliageWeather : TegeBehavior {
    float t = 0.0f;
    int phase = -1;
    float rain = 0.0f, snow = 0.0f;   // eased current
    float rainT = 0.0f, snowT = 0.0f; // per-phase targets

    void OnStart() {
        Subtitle_Show("Foliage demo - weather evolves on its own", "", 3.5f);
    }

    void OnUpdate(float dt) {
        t += dt;
        int p = int(t / 15.0f) % 4;   // 15s per phase
        if (p != phase) {
            phase = p;
            // Weather_SetWind slants precipitation; Wind_SetStrength/Direction gusts the
            // foliage (trees/grass/shrubs/imported meshes) via the wind system.
            if (p == 0) { rainT = 0.0f; snowT = 0.0f; Weather_Set(0, 3.0f); Weather_SetWind(0.8, 0.0, 0.4, 2.0); Wind_SetDirection(0.8, 0.0, 0.4); Wind_SetStrength(1.0f); Subtitle_Show("Clear", "", 2.0f); }
            if (p == 1) { rainT = 0.0f; snowT = 0.0f; Weather_Set(1, 3.0f); Weather_SetWind(1.0, 0.0, 0.5, 8.0); Wind_SetDirection(1.0, 0.0, 0.5); Wind_SetStrength(5.5f); Subtitle_Show("Wind picking up", "", 2.0f); }
            if (p == 2) { rainT = 0.85f; snowT = 0.0f; Weather_Set(2, 3.0f); Weather_SetWind(0.9, 0.0, 0.6, 9.0); Wind_SetDirection(0.9, 0.0, 0.6); Wind_SetStrength(4.5f); Render_SetRainActive(true); Subtitle_Show("Rain", "", 2.0f); }
            if (p == 3) { rainT = 0.0f; snowT = 0.8f; Weather_Set(4, 3.0f); Weather_SetWind(0.5, 0.0, 0.3, 5.0); Wind_SetDirection(0.5, 0.0, 0.3); Wind_SetStrength(2.0f); Subtitle_Show("Snow", "", 2.0f); }
        }
        rain += (rainT - rain) * min(dt * 0.6f, 1.0f);
        snow += (snowT - snow) * min(dt * 0.6f, 1.0f);
        Weather_SetRainIntensity(rain);
        Weather_SetSnowIntensity(snow);
        if (rain < 0.02f && rainT == 0.0f) Render_SetRainActive(false);
    }

    float min(float a, float b) { return a < b ? a : b; }
}
