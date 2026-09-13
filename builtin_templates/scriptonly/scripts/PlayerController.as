// PlayerController.as - the player, and the only script that owns their state.
//
// The hazard, the pickups and the portal cannot call into this class: every .as
// is its own module. So this LISTENS for what they did and ANNOUNCES what
// changed, which is the only shape that works across modules.
class PlayerController : TegeBehavior {
    [Property] float moveSpeed = 5.0f;
    [Property] float maxHealth = 100.0f;

    float currentHealth = 100.0f;
    int pickupsCollected = 0;

    void OnStart() {
        currentHealth = maxHealth;
        Events_Listen("player_damage", EventCallback(this.OnDamage));
        Events_Listen("player_pickup", EventCallback(this.OnPickup));
        UpdateHUD();
    }

    void OnDamage(const string &in ev) { TakeDamage(Events_CurrentFloat("amount")); }
    void OnPickup(const string &in ev) { AddPickup(); }

    // Anything that needs to know the count hears it here rather than reading
    // it off this object, which no other module can do.
    void Announce() {
        EventData@ d = EventData();
        d.SetInt("pickups", pickupsCollected);
        d.SetInt("health", int(currentHealth));
        Events_Send("player_state", d);
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
        Announce();
        uint64 hud = Scene_FindEntity("HUD_Text");
        if (hud != 0) {
            HUD_SetText(hud, "HP: " + int(currentHealth) + "/100  |  Pickups: " + pickupsCollected + "/3");
        }
    }
}
