// Clap, and listen to the room answer.
//
// A reverb tail is what you hear AFTER a sound stops, which is why three
// continuous projector hums were the wrong instrument for this demo: a drone
// fills the gap the tail is supposed to live in, and every room ends up sounding
// like a drone. A room is auditioned with a TRANSIENT -- a clap, a starter
// pistol, a snapped finger -- and then silence to hear it decay into.
//
//   Q       clap where you are standing
//   P       projectors on and off
//   R       say where the listener is
//
// Q rather than SPACE because SPACE is Jump, and a demo where auditioning the
// room also launches you into the ceiling is a demo you fight. The keys still
// free with a third-person controller in the scene are Q, R, and P: WASD and the
// arrows are movement, SPACE is jump, SHIFT is sprint, CTRL and C are crouch, E
// is interact, and the editor takes F for Focus even while playing.
//
// Every binding used here was checked against the engine's registration list
// before it was written. The first draft called Entity_FindByName,
// Camera_GetPosition and Log, none of which exist -- which is the same mistake
// as documentation that teaches an API the engine does not have, and this repo
// already has a commit about that.

class RoomClap : TegeBehavior {

    [Property] float clapCooldown = 0.35f;   // seconds between claps

    float sinceClap = 10.0f;
    bool projectorsOn = true;

    uint64 player = 0;
    uint64 kitchenProjector = 0;
    uint64 hallProjector = 0;
    uint64 basementProjector = 0;

    void OnStart() {
        player = Scene_FindEntity("Player");
        kitchenProjector = Scene_FindEntity("Projector - tiled kitchen");
        hallProjector = Scene_FindEntity("Projector - wooden hall");
        basementProjector = Scene_FindEntity("Projector - carpeted basement");

        if (player == 0) {
            Debug_LogError("RoomClap: no entity named Player -- claps will land at the origin");
        }
        Debug_Log("RoomClap ready.  Q = clap,  P = projectors on/off,  R = where am I");
    }

    void OnUpdate(float dt) {
        sinceClap += dt;

        // A clap where the listener is, so the room you are standing in is the
        // room that answers.
        if (Input_GetKeyDown(Key::Q) && sinceClap >= clapCooldown) {
            sinceClap = 0.0f;
            Vector3 here = Vector3(0, 1.6f, 0);
            if (player != 0) here = Entity_GetPosition(player);
            Audio_PlayAtPosition("assets/clap.wav", here);
        }

        // The projectors are the "sits against the back wall" cue, and they are
        // also the thing that makes a tail hard to hear. Being able to shut them
        // up is most of what makes this demo usable.
        if (Input_GetKeyDown(Key::P)) {
            projectorsOn = !projectorsOn;
            if (projectorsOn) {
                if (kitchenProjector != 0) Audio_Play(kitchenProjector);
                if (hallProjector != 0) Audio_Play(hallProjector);
                if (basementProjector != 0) Audio_Play(basementProjector);
                Debug_Log("Projectors on");
            } else {
                if (kitchenProjector != 0) Audio_Stop(kitchenProjector);
                if (hallProjector != 0) Audio_Stop(hallProjector);
                if (basementProjector != 0) Audio_Stop(basementProjector);
                Debug_Log("Projectors off -- clap and listen to the tail");
            }
        }

        if (Input_GetKeyDown(Key::R)) {
            if (player != 0) {
                Vector3 p = Entity_GetPosition(player);
                Debug_Log("Listener at " + p.x + ", " + p.y + ", " + p.z);
            }
        }
    }
}
