// WebDemoInput.as — Tab switches between playing and the accessibility menu.
// Play mode: cursor captured, WASD + mouse look. Menu mode: cursor free,
// the settings panel slides in from the right, everything is clickable.
class WebDemoInput : TegeBehavior {
    uint64 menuCanvas = 0;
    uint64 hintCanvas = 0;
    uint64 player = 0;
    float slide = 1.0f;        // 0 = menu on screen, 1 = parked off the right edge
    float slideTarget = 1.0f;
    float pulseT = 0.0f;
    bool menuOpen = false;

    void OnStart() {
        menuCanvas = Scene_FindEntity("Accessibility Menu");
        hintCanvas = Scene_FindEntity("Tab Hint");
        player = Scene_FindEntity("Player");
        // Park the menu off screen before the first frame renders
        if (menuCanvas != 0) UI_SetElementOffsets(menuCanvas, 1, 900.0f, 900.0f, 0.0f, 0.0f);
        Subtitle_Show("WASD to move, mouse to look. Press Tab for accessibility settings.", "", 5.0f);
    }

    void OnUpdate(float dt) {
        if (Input_GetKeyDown(Key::Tab)) {
            menuOpen = !menuOpen;
            slideTarget = menuOpen ? 0.0f : 1.0f;
            Input_SetMouseCaptured(!menuOpen);
            // Menu mode = the character stops listening entirely: no WASD
            // drift, no camera spin while the cursor crosses the panel.
            if (player != 0) Controller_SetEnabled(player, !menuOpen);
            if (menuOpen) {
                Announcer_Announce("Accessibility settings open");
            } else {
                Subtitle_Show("Back to play mode", "", 1.5f);
            }
        }

        // Ease the panel toward its target (springy slide from the right)
        float step = dt * 7.0f;
        if (step > 1.0f) step = 1.0f;
        slide += (slideTarget - slide) * step;
        if (slide < 0.001f) slide = 0.0f;
        if (slide > 0.999f) slide = 1.0f;
        if (menuCanvas != 0) {
            float off = slide * 900.0f;
            UI_SetElementOffsets(menuCanvas, 1, off, off, 0.0f, 0.0f);
        }

        // Breathing glow on the Tab pill; hide it while the menu is open
        pulseT += dt;
        if (pulseT > 2.4f) pulseT -= 2.4f;
        if (hintCanvas != 0) {
            UI_SetElementVisible(hintCanvas, 1, !menuOpen);
            UI_SetElementVisible(hintCanvas, 2, !menuOpen);
            float half = 1.2f;
            float tri = pulseT < half ? pulseT / half : (2.4f - pulseT) / half;
            UI_SetBgColor(hintCanvas, 1, 0.10f, 0.42f, 0.46f, 0.55f + 0.30f * tri);
        }
    }
}
