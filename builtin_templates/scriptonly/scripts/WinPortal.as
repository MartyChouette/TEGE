// WinPortal.as — Bite-sized behavior attached to the Win Portal object
class WinPortal : TegeBehavior {
    [Property] float portalRadius = 1.8f;
    [Property] int requiredPickups = 3;

    void OnUpdate(float dt) {
        Vector3 rot = GetRotation();
        rot.z += 45.0f * dt;
        SetRotation(rot);

        uint64 player = Scene_FindEntity("Player");
        if (player != 0) {
            Vector3 diff = GetPosition() - Entity_GetPosition(player);
            if (diff.Length() <= portalRadius) {
                PlayerController@ controller = cast<PlayerController>(Entity_GetBehavior(player, "PlayerController"));
                if (controller !is null) {
                    uint64 hud = Scene_FindEntity("HUD_Text");
                    if (controller.pickupsCollected >= requiredPickups) {
                        if (hud != 0) HUD_SetText(hud, "YOU WIN! All " + requiredPickups + " Pickups Collected!");
                        Debug_Log("VICTORY! Reached portal with all pickups.");
                    } else {
                        if (hud != 0) HUD_SetText(hud, "Need " + requiredPickups + " Pickups to Win! (" + controller.pickupsCollected + "/" + requiredPickups + ")");
                    }
                }
            }
        }
    }
}
