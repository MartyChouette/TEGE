// WinPortal.as - the exit, once enough is collected
//
// ONE SCRIPT CANNOT REACH INTO ANOTHER. Every .as file compiles as its own
// module, so a class defined in PlayerController.as is not a type this file can
// name, and Entity_GetBehavior -- which these scripts used to call -- is not a
// binding this engine has ever had. They therefore never compiled, and the
// template shipped teaching a pattern that cannot work.
//
// Scripts talk through EVENTS instead, which is the mechanism the engine
// actually provides: Events_Send with an EventData payload, Events_Listen with
// a handler taking (const string &in).
class WinPortal : TegeBehavior {
    [Property] float portalRadius = 1.8f;
    [Property] int requiredPickups = 3;

    // The count cannot be read off the player's script, so the player ANNOUNCES
    // it and this keeps the last value it heard. One extra line, and it is the
    // only shape that works across modules.
    private int _pickups = 0;

    void OnStart() {
        Events_Listen("player_state", EventCallback(this.OnPlayerState));
    }

    void OnPlayerState(const string &in ev) {
        _pickups = Events_CurrentInt("pickups");
    }

    void OnUpdate(float dt) {
        Vector3 rot = GetRotation();
        rot.z += 45.0f * dt;
        SetRotation(rot);

        uint64 player = Scene_FindEntity("Player");
        if (player == 0) return;

        Vector3 diff = GetPosition() - Entity_GetPosition(player);
        if (diff.Length() > portalRadius) return;

        uint64 hud = Scene_FindEntity("HUD_Text");
        if (hud == 0) return;
        if (_pickups >= requiredPickups) {
            HUD_SetText(hud, "YOU WIN! All " + requiredPickups + " Pickups Collected!");
            Debug_Log("VICTORY! Reached portal with all pickups.");
        } else {
            HUD_SetText(hud, "Need " + requiredPickups + " Pickups to Win! ("
                             + _pickups + "/" + requiredPickups + ")");
        }
    }
}
