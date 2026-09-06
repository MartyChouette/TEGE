// PlayerController.as — Bite-sized behavior attached to Player
class PlayerController : TegeBehavior {
    [Property] float moveSpeed = 5.0f;
    [Property] float maxHealth = 100.0f;

    float currentHealth = 100.0f;
    int pickupsCollected = 0;

    void OnStart() {
        currentHealth = maxHealth;
        UpdateHUD();
    }

    void OnUpdate(float dt) {
        Vector3 move = Vector3(0, 0, 0);
        if (Input_GetKey(Key::W)) move.z -= 1.0f;
        if (Input_GetKey(Key::S)) move.z += 1.0f;
        if (Input_GetKey(Key::A)) move.x -= 1.0f;
        if (Input_GetKey(Key::D)) move.x += 1.0f;

        if (move.Length() > 0.001f) {
            move = move.Normalized() * (moveSpeed * dt);
            SetPosition(GetPosition() + move);
        }
    }

    void AddPickup() {
        pickupsCollected++;
        UpdateHUD();
        Debug_Log("Collected pickup! Total: " + pickupsCollected);
    }

    void TakeDamage(float damage) {
        currentHealth -= damage;
        if (currentHealth < 0.0f) currentHealth = 0.0f;
        UpdateHUD();
        Debug_Log("Player took " + damage + " damage. HP: " + currentHealth);
        if (currentHealth <= 0.0f) {
            uint64 hud = Scene_FindEntity("HUD_Text");
            if (hud != 0) HUD_SetText(hud, "GAME OVER! Health Depleted.");
        }
    }

    void UpdateHUD() {
        uint64 hud = Scene_FindEntity("HUD_Text");
        if (hud != 0) {
            HUD_SetText(hud, "HP: " + int(currentHealth) + "/100  |  Pickups: " + pickupsCollected + "/3");
        }
    }
}
