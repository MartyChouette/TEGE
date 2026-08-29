// Water FX demo (F2-F5): everything visible is AUTHORED looped motion riding
// the F1 material primitive - no runtime simulation. The runoff strips are
// weather-linked the scripted way: visible only while it rains.
class WaterFXDemo : TegeBehavior {
    bool raining = false;
    void OnStart() {
        SetRunoff(false);
        Debug_Log("WaterFX: R toggles rain (roof runoff strips)");
    }
    void SetRunoff(bool on) {
        for (int i = 0; i < 3; i++) {
            uint64 e = Scene_FindEntity("Runoff" + i);
            if (e != 0) Entity_SetVisible(e, on);
        }
    }
    void OnUpdate(float dt) {
        if (Input_GetKeyDown(Key::R)) {
            raining = !raining;
            Render_SetRainActive(raining);
            Weather_SetRainIntensity(raining ? 0.8f : 0.0f);
            SetRunoff(raining);
            Debug_Log(raining ? "Rain ON - runoff flowing" : "Rain OFF");
        }
    }
}
