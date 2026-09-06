// AccessibilityDemo.as — applies UI control changes to the engine LIVE.
// Works identically in the editor, exported desktop games, and the browser
// (where Announcer_Announce is spoken aloud via the Web Speech API).
class AccessibilityDemo : TegeBehavior {
    array<string> modeNames = {"Off", "Protanopia", "Deuteranopia", "Tritanopia",
                               "Protanomaly", "Deuteranomaly", "Tritanomaly",
                               "Achromatopsia", "Achromatomaly"};
    int lastMode = 0;

    void OnStart() {
        Events_Listen("a11y_cb_mode", EventCallback(this.OnColorblindMode));
        Events_Listen("a11y_cb_strength", EventCallback(this.OnColorblindStrength));
        Events_Listen("a11y_font_scale", EventCallback(this.OnFontScale));
        Events_Listen("a11y_dyslexia", EventCallback(this.OnDyslexia));
        Events_Listen("a11y_reader", EventCallback(this.OnScreenReader));
        Events_Listen("a11y_reader_test", EventCallback(this.OnReaderTest));
        Events_Listen("a11y_subtitles", EventCallback(this.OnSubtitles));
        Events_Listen("a11y_subtitle_size", EventCallback(this.OnSubtitleSize));
        Events_Listen("a11y_reduced_motion", EventCallback(this.OnReducedMotion));
        Events_Listen("a11y_brightness", EventCallback(this.OnBrightness));
        Events_Listen("a11y_contrast", EventCallback(this.OnContrast));
        Events_Listen("a11y_save", EventCallback(this.OnSave));

        // Sync every control to the ACTUAL loaded settings (accessibility.json
        // etc.) — without this the toggles draw their design-time defaults and
        // lie about the real state (e.g. Subtitles shown ON while disabled).
        uint64 menu = Scene_FindEntity("Accessibility Menu");
        if (menu != 0) {
            UI_SetSliderValue(menu, 4, float(Colorblind_GetMode()));
            UI_SetSliderValue(menu, 6, Colorblind_GetStrength());
            UI_SetSliderValue(menu, 8, Accessibility_GetFontScale());
            UI_SetChecked(menu, 10, Accessibility_GetDyslexiaFont());
            UI_SetChecked(menu, 12, Announcer_IsEnabled());
            UI_SetChecked(menu, 15, Subtitle_IsEnabled());
            UI_SetSliderValue(menu, 17, Subtitle_GetFontSize());
            UI_SetChecked(menu, 19, Accessibility_GetReducedMotion());
            UI_SetSliderValue(menu, 21, Accessibility_GetBrightness());
            UI_SetSliderValue(menu, 23, Accessibility_GetContrast());
            lastMode = Colorblind_GetMode();
        }
        Subtitle_Show("Accessibility demo ready. Navigate with Tab, arrows, or gamepad.", "", 4.0f);
    }

    void OnColorblindMode(const string &in ev) {
        int mode = int(Events_CurrentFloat("value") + 0.5f);
        if (mode < 0) mode = 0;
        if (mode > 8) mode = 8;
        Colorblind_SetMode(mode);
        if (mode != lastMode) {
            lastMode = mode;
            Subtitle_Show("Colorblind mode: " + modeNames[mode], "", 2.0f);
            Announcer_Announce("Colorblind mode " + modeNames[mode]);
        }
    }

    void OnColorblindStrength(const string &in ev) {
        Colorblind_SetStrength(Events_CurrentFloat("value"));
    }

    void OnFontScale(const string &in ev) {
        Accessibility_SetFontScale(Events_CurrentFloat("value"));
    }

    void OnDyslexia(const string &in ev) {
        bool on = Events_CurrentInt("checked") != 0;
        Accessibility_SetDyslexiaFont(on);
        Subtitle_Show(on ? "Dyslexia-friendly text on" : "Dyslexia-friendly text off", "", 2.0f);
    }

    void OnScreenReader(const string &in ev) {
        bool on = Events_CurrentInt("checked") != 0;
        Announcer_SetEnabled(on);
        if (on) Announcer_Announce("Screen reader enabled");
        Subtitle_Show(on ? "Screen reader on" : "Screen reader off", "", 2.0f);
    }

    void OnReaderTest(const string &in ev) {
        Announcer_AnnounceHighPriority("This is a test announcement. In the browser this is spoken aloud.");
    }

    void OnSubtitles(const string &in ev) {
        bool on = Events_CurrentInt("checked") != 0;
        Subtitle_SetEnabled(on);
        if (on) Subtitle_Show("Subtitles enabled", "", 2.0f);
    }

    void OnSubtitleSize(const string &in ev) {
        Subtitle_SetFontSize(Events_CurrentFloat("value"));
        Subtitle_Show("Subtitle size " + int(Events_CurrentFloat("value")), "", 1.5f);
    }

    void OnReducedMotion(const string &in ev) {
        bool on = Events_CurrentInt("checked") != 0;
        Accessibility_SetReducedMotion(on);
        Subtitle_Show(on ? "Reduced motion on" : "Reduced motion off", "", 2.0f);
    }

    void OnBrightness(const string &in ev) {
        Accessibility_SetBrightness(Events_CurrentFloat("value"));
    }

    void OnContrast(const string &in ev) {
        Accessibility_SetContrast(Events_CurrentFloat("value"));
    }

    void OnSave(const string &in ev) {
        Accessibility_SaveSettings();
        Subtitle_Show("Settings saved", "", 2.0f);
        Announcer_Announce("Settings saved");
    }
}
