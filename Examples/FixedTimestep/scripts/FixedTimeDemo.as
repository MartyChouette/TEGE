// Fixed-timestep + time-scale demo. The project runs physics at a fixed 60Hz
// tick (Settings > Project > Fixed Physics Timestep), so the crates behave the
// same at any frame rate - try different editor FPS caps. Slow-mo just runs
// more ticks per second of game time; the solver quality never changes.
class FixedTimeDemo : TegeBehavior {
    int fixedTicks = 0;

    void OnFixedUpdate(float dt) {
        // Runs exactly once per physics tick (60Hz of GAME time - slow-mo
        // makes these arrive slower in wall time, never smaller).
        fixedTicks++;
    }

    void OnUpdate(float dt) {
        if (Input_GetKeyDown(Key::Num1)) {
            Time_SetScale(0.25f);
            Debug_Log("Time scale 0.25x (slow-mo) - fixed ticks so far: " + fixedTicks);
        }
        if (Input_GetKeyDown(Key::Num2)) {
            Time_SetScale(0.05f);
            Debug_Log("Time scale 0.05x (hitstop) - fixed ticks so far: " + fixedTicks);
        }
        if (Input_GetKeyDown(Key::Num3)) {
            Time_SetScale(1.0f);
            Debug_Log("Time scale 1.0x (normal) - fixed ticks so far: " + fixedTicks);
        }
        if (Input_GetKeyDown(Key::E)) {
            // Kick every crate toward +Y with some sideways scatter
            for (int i = 0; i < 10; i++) {
                uint64 crate = Scene_FindEntity("Crate" + i);
                if (crate == 0) continue;
                float side = (i % 2 == 0) ? 1.0f : -1.0f;
                Physics_AddImpulse(crate, Vector3(side * RandomRange(0.5f, 2.5f),
                                                  RandomRange(4.0f, 8.0f),
                                                  RandomRange(-1.5f, 1.5f)));
            }
            Debug_Log("Kicked the crates");
        }
        if (Input_GetKeyDown(Key::R)) {
            Time_SetScale(1.0f);
            Scene_Restart();
        }
    }
}
