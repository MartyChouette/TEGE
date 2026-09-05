// Calls every generator binding once.
//
// This is a registration probe, not a behaviour test. AngelScript resolves a
// global function at COMPILE time, so a binding that was never registered makes
// this whole module fail to compile and the log says exactly which name is
// missing. That is the check the Difficulty_* API needed and never had: it was
// documented in SCRIPTING_API.md for months while existing only as a comment
// block in a header, and nothing in the tree would have noticed.
class ApiProbe : TegeBehavior {
    bool ran = false;

    void OnStart() {
        Debug_Log("ApiProbe: resolving every generator binding");

        uint64 blob    = Scene_FindEntity("Blob0");
        uint64 surface = Scene_FindEntity("MetaballSurface");
        uint64 life    = Scene_FindEntity("LifeWall");
        uint64 tess    = Scene_FindEntity("Tesseract");
        uint64 star    = Scene_FindEntity("FourierStar");

        // --- Metaballs ---
        Metaball_SetRadius(blob, 1.5f);
        Metaball_SetStrength(blob, 1.0f);
        Metaball_SetGroup(blob, 0);
        Metaball_SetColor(blob, 0.1f, 1.0f, 0.8f);
        MetaballSurface_SetGroup(surface, 0);
        MetaballSurface_SetGridResolution(surface, 48);
        MetaballSurface_SetGridSize(surface, 12.0f);

        // --- Cellular automata ---
        CA_SetRunning(life, true);
        CA_SetRule(life, 1);
        CA_SetStampPattern(life, "");
        CA_Reset(life);
        uint gen  = CA_GetGeneration(life);
        uint live = CA_GetLiveCells(life);

        // --- 4D projection ---
        P4D_SetPolytope(tess, 0);
        P4D_SetRotation(tess, 0.0f, 0.0f, 0.45f, 0.0f, 0.27f, 0.0f);
        P4D_SetScale(tess, 2.4f);
        P4D_SetAnimate(tess, true);

        // --- Fourier ---
        Fourier_SetContour(star, 2);
        Fourier_SetTerms(star, 24);
        Fourier_SetExtrude(star, 0.6f);
        int terms = Fourier_GetActiveTerms(star);

        // --- Texture simulations (no entity in this scene; the bindings must
        //     still resolve, and each is a safe no-op on a missing component) ---
        RD_SetPreset(0, 4);
        RD_SetSettleSteps(0, 2000);
        RD_Rebake(0);
        uint rdSteps = RD_GetStepCount(0);

        Physarum_SetPreset(0, 1);
        Physarum_SetAgentCount(0, 20000);
        Physarum_SetSettleSteps(0, 800);
        Physarum_Rebake(0);
        uint phSteps = Physarum_GetStepCount(0);

        // --- Freeze switches ---
        ProceduralMesh_SetRegenerate(tess, true);
        ProceduralTexture_SetRegenerate(0, true);

        // --- Dynamic difficulty: documented for months, registered only now ---
        uint64 self = _entityId;
        Difficulty_SetEnabled(self, true);
        Difficulty_SetPlayerEntity(self, self);
        Difficulty_SetBaseDifficulty(self, 1);
        Difficulty_RecordDeath(self);
        Difficulty_RecordHit(self);
        Difficulty_RecordShot(self);
        Difficulty_RecordCheckpointHealth(self, 0.75f);
        Difficulty_SetResourceRatio(self, 0.5f);
        Difficulty_Reset(self);
        float score = Difficulty_GetScore(self);
        float mult  = Difficulty_GetMultiplier(self, "enemyDamage");
        uint  baseD = Difficulty_GetBaseDifficulty(self);

        Debug_Log("ApiProbe: ALL BINDINGS RESOLVED"
                  " ca(gen=" + gen + ",live=" + live + ")"
                  " fourier(terms=" + terms + ")"
                  " rd(steps=" + rdSteps + ")"
                  " physarum(steps=" + phSteps + ")"
                  " difficulty(score=" + score + ",mult=" + mult + ",base=" + baseD + ")");
        ran = true;
    }
}
