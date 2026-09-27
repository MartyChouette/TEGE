// Exercises the dynamic difficulty system and the Difficulty_* script bindings.
//
// Two things were broken here before this demo existed. The system's Update was
// never called by any runtime, so the struggle score never recomputed. And the
// Difficulty_* API was documented in SCRIPTING_API.md while existing only as a
// comment in the header, so every call in a user script failed to compile.
class DifficultyProbe : TegeBehavior {
    uint64 self = 0;
    uint64 player = 0;
    float reportTimer = 0.0f;

    // Project actions (Project Settings > Input & Touch), keys 1-3 and R by
    // default, so they can be rebound and show in the controls screen.
    int actDeath = -1;
    int actHit = -1;
    int actShot = -1;
    int actReset = -1;

    void OnStart() {
        actDeath = InputAction_Find("Record Death");
        actHit   = InputAction_Find("Record Hit");
        actShot  = InputAction_Find("Record Shot");
        actReset = InputAction_Find("Reset Difficulty");
        self = _entityId;
        player = Scene_FindEntity("Player");
        if (player != 0) Difficulty_SetPlayerEntity(self, player);

        Debug_Log("DifficultyProbe ready");
        Debug_Log("  1 = record death, 2 = record hit, 3 = record shot, R = reset");
        Debug_Log("  starting score " + Difficulty_GetScore(self));
    }

    void OnUpdate(float dt) {
        if (InputAction_IsPressed(actDeath)) {
            Difficulty_RecordDeath(self);
            Debug_Log("death recorded");
        }
        if (InputAction_IsPressed(actHit)) {
            Difficulty_RecordHit(self);
            Debug_Log("hit recorded");
        }
        if (InputAction_IsPressed(actShot)) {
            Difficulty_RecordShot(self);
            Debug_Log("shot recorded");
        }
        if (InputAction_IsPressed(actReset)) {
            Difficulty_Reset(self);
            Debug_Log("metrics reset");
        }

        // The system recomputes once per second, so sampling faster than that
        // just reprints the same number.
        reportTimer += dt;
        if (reportTimer >= 2.0f) {
            reportTimer = 0.0f;
            Debug_Log("score=" + Difficulty_GetScore(self)
                      + "  enemyDamage x" + Difficulty_GetMultiplier(self, "enemyDamage")
                      + "  enemyHealth x" + Difficulty_GetMultiplier(self, "enemyHealth"));
        }
    }
}
