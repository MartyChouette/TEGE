// PortraitRig.as - one bust, built from swappable layers, driven by an emotion NAME.
//
// SHARED. Nothing in here knows what game it is in. It takes a string like
// "worried" and shows the right stack of layer entities. That is the whole
// contract, and it is why one file serves a conversation, an interrogation or
// a negotiation in different projects without a line of difference.
//
// THE SCENE HOLDS THE PIECES, THE RIG PICKS THEM. Runtime texture swapping is
// not a thing in TEGE, so every layer piece exists as its own entity and the
// rig toggles visibility. Attach this to the portrait ROOT; name the children by
// convention:
//
//   base_head            brow_neutral  brow_raised  brow_furrowed
//   brow_worried         brow_skeptical
//   eye_open  eye_half   eye_happy  eye_wide  eye_left  eye_right
//   eye_down  eye_teary  eye_squeezed  eye_closed
//   mouth_neutral  mouth_smile  mouth_open-smile  mouth_big-open  mouth_frown
//   mouth_small-o  mouth_gritted  mouth_pout  mouth_wavy
//   mouth_mid                      (lip-sync only: the halfway mouth)
//   blush_light  blush_heavy
//   fx_sweat  fx_anger  fx_tears  fx_sparkle  fx_gloom  fx_excl  fx_quest
//
// MISSING PIECES ARE FINE. A layer with no entity is skipped and recorded in
// Missing(), so a rig runs on two drawings and tells you what is not painted
// yet. That is the point: the emotion table can be finished long before the
// art is.
//
// Per CharacterExpressionSpec.md: the 10 MVP emotions ship on everyone, the
// full 16 on major characters. Auto-blink and look-toward are in here too,
// because the spec is right that they sell "alive" harder than emotion 17 does.
#include "TegeBehavior.as"

// The 16 emotions, each as "brow|eye|mouth|blush|fx". "-" means that layer is
// off for this emotion. THIS TABLE IS THE ONLY PLACE THE COMBOS LIVE. A
// character with different art overrides emoteTable; it does not fork the rig.
const string PORTRAIT_EMOTES =
    "neutral=neutral|open|neutral|-|-;"
    "smile=neutral|happy|smile|-|-;"
    "laugh=raised|happy|big-open|-|-;"
    "sad=worried|down|frown|-|-;"
    "angry=furrowed|wide|gritted|-|anger;"
    "surprised=raised|wide|small-o|-|excl;"
    "blush=worried|right|wavy|heavy|sweat;"
    "worried=worried|half|wavy|-|-;"
    "smug=skeptical|half|smile|-|-;"
    "flustered=raised|squeezed|open-smile|heavy|sweat;"
    "crying=worried|teary|frown|-|tears;"
    "thoughtful=furrowed|left|neutral|-|quest;"
    "disgusted=furrowed|half|pout|-|-;"
    "sleepy=neutral|half|small-o|-|gloom;"
    "flirty=skeptical|half|smile|light|sparkle;"
    "determined=furrowed|open|neutral|-|-";

class PortraitRig : TegeBehavior {
    // Override to give one character a different set of combos. Same format as
    // PORTRAIT_EMOTES; anything not named here falls back to the shared table.
    [Property] string emoteTable = "";
    [Property] string startEmote = "neutral";

    // WHO THIS BUST IS. A driver does not need a handle on this script; it
    // sends "portrait_emote" with who + emote and every rig in the scene checks
    // whether the name is its own. That keeps a driver from having to reach
    // across entities to move a face.
    [Property] string speakerId = "";

    // TWO BUSTS IN ONE SCENE. Layers are found by NAME through Scene_FindEntity,
    // which is scene-global, so a second rig would grab the first rig's pieces.
    // Give each rig a prefix and name its children "L_brow_neutral",
    // "R_brow_neutral" and so on. A scene with one bust can leave it empty.
    [Property] string layerPrefix = "";

    // Eye life. blinkEvery 0 turns blinking off, for a screenshot or for a
    // character whose eyes are covered.
    [Property] float blinkEvery = 3.4f;
    [Property] float blinkHold  = 0.09f;

    // Lip-sync, 3-phase (closed / mid / open). The spec says three is enough
    // when the words are text anyway. Speak(true) while the line is arriving.
    [Property] float mouthRate = 11.0f;

    array<string> layerName;      // every piece this rig knows how to show
    array<uint64> layerEnt;       // 0 = not in the scene
    array<string> absent;         // the pieces the art has not caught up to

    string emote = "";
    array<string> want;           // the layer names the current emote asks for
    float blinkT = 0.0f;
    bool  blinking = false;
    bool  talking = false;
    float mouthT = 0.0f;
    bool  mouthOpen = false;

    // ======================================================================
    void OnStart() {
        // Every piece either table mentions, plus the always-on base and the
        // lip-sync mouth, is looked up once. Scene_FindEntity is not free and
        // an emote change has to cost nothing.
        Claim("base_head");
        Claim("mouth_mid");
        Claim("eye_closed");
        Learn(PORTRAIT_EMOTES);
        if (emoteTable != "") Learn(emoteTable);

        Show("base_head", true);
        Set(startEmote);

        Events_Listen("portrait_emote", EventCallback(this.OnEmoteEvent));
        Events_Listen("portrait_speak", EventCallback(this.OnSpeakEvent));
        Events_Listen("portrait_look",  EventCallback(this.OnLookEvent));
    }

    // A rig with no speakerId answers to everything, which is what a debug
    // scene with one bust in it wants.
    bool Mine(const string &in who) {
        return speakerId == "" || who == "" || who == speakerId;
    }

    void OnEmoteEvent(const string &in ev) {
        if (Mine(Events_CurrentString("who"))) Set(Events_CurrentString("emote"));
    }

    void OnSpeakEvent(const string &in ev) {
        if (Mine(Events_CurrentString("who"))) Speak(Events_CurrentInt("on") != 0);
    }

    void OnLookEvent(const string &in ev) {
        if (Mine(Events_CurrentString("who"))) LookAt(Events_CurrentInt("dir"));
    }

    // Register every layer piece a table mentions.
    void Learn(const string &in table) {
        array<string> rows = Split(table, ";");
        for (uint i = 0; i < rows.length(); i++) {
            int eq = rows[i].findFirst("=");
            if (eq < 0) continue;
            array<string> parts = Split(rows[i].substr(eq + 1), "|");
            for (uint p = 0; p < parts.length() && p < 5; p++) {
                if (parts[p] == "" || parts[p] == "-") continue;
                Claim(Prefix(p) + parts[p]);
            }
        }
    }

    // Private, not global. A shared file must never introduce a global that
    // shadows or collides with a host script's own member, and game scripts
    // commonly define their own Split. enjin_api/StrUtil.as has these as
    // globals for scripts that want them.
    array<string> Split(const string &in s, const string &in sep) {
        array<string> parts;
        if (sep.length() == 0) { parts.insertLast(s); return parts; }
        int start = 0;
        while (true) {
            int at = s.findFirst(sep, uint(start));
            if (at < 0) {
                parts.insertLast(s.substr(uint(start), s.length() - uint(start)));
                break;
            }
            parts.insertLast(s.substr(uint(start), uint(at - start)));
            start = at + int(sep.length());
        }
        return parts;
    }

    string Prefix(uint slot) {
        if (slot == 0) return "brow_";
        if (slot == 1) return "eye_";
        if (slot == 2) return "mouth_";
        if (slot == 3) return "blush_";
        return "fx_";
    }

    void Claim(const string &in name) {
        for (uint i = 0; i < layerName.length(); i++)
            if (layerName[i] == name) return;
        uint64 e = Scene_FindEntity(layerPrefix + name);
        layerName.insertLast(name);
        layerEnt.insertLast(e);
        if (e == 0) absent.insertLast(name);
        else Entity_SetVisible(e, false);
    }

    void Show(const string &in name, bool on) {
        for (uint i = 0; i < layerName.length(); i++) {
            if (layerName[i] != name) continue;
            if (layerEnt[i] != 0) Entity_SetVisible(layerEnt[i], on);
            return;
        }
    }

    // ======================================================================
    // THE CONTRACT. One call, one string.
    void Set(const string &in name) {
        if (name == emote) return;
        string row = Lookup(emoteTable, name);
        if (row == "") row = Lookup(PORTRAIT_EMOTES, name);
        if (row == "") return;             // unknown emote: hold the last one

        for (uint i = 0; i < want.length(); i++) Show(want[i], false);
        want.resize(0);

        array<string> parts = Split(row, "|");
        for (uint p = 0; p < parts.length() && p < 5; p++) {
            if (parts[p] == "" || parts[p] == "-") continue;
            string n = Prefix(p) + parts[p];
            want.insertLast(n);
            Show(n, true);
        }
        emote = name;
        blinking = false;
        blinkT = 0.0f;
    }

    string Lookup(const string &in table, const string &in name) {
        if (table == "") return "";
        array<string> rows = Split(table, ";");
        for (uint i = 0; i < rows.length(); i++) {
            int eq = rows[i].findFirst("=");
            if (eq < 0) continue;
            if (rows[i].substr(0, eq) == name) return rows[i].substr(eq + 1);
        }
        return "";
    }

    string Emote() { return emote; }

    // What the art still owes you. Empty means the rig is fully dressed.
    string Missing() {
        string s = "";
        for (uint i = 0; i < absent.length(); i++)
            s += (i > 0 ? ", " : "") + absent[i];
        return s;
    }

    int MissingCount() { return int(absent.length()); }

    // The whole vocabulary, for a debug scene that wants to cycle it.
    array<string> Emotes() {
        array<string> names;
        array<string> rows = Split(PORTRAIT_EMOTES, ";");
        for (uint i = 0; i < rows.length(); i++) {
            int eq = rows[i].findFirst("=");
            if (eq > 0) names.insertLast(rows[i].substr(0, eq));
        }
        return names;
    }

    // ======================================================================
    // The mouth moves while a line is arriving, and settles back onto the
    // emotion's own mouth when it stops.
    void Speak(bool on) {
        if (talking == on) return;
        talking = on;
        if (!on) {
            Show("mouth_mid", false);
            MouthOfEmote(true);
            mouthOpen = false;
        }
        mouthT = 0.0f;
    }

    void MouthOfEmote(bool on) {
        for (uint i = 0; i < want.length(); i++)
            if (want[i].substr(0, 6) == "mouth_") Show(want[i], on);
    }

    // Look toward whoever is talking: -1 left, 0 ahead, 1 right, 2 down. Only
    // moves eyes that are plainly open, because teary / squeezed / happy-closed
    // are all carrying the expression and must not be overwritten by a glance.
    void LookAt(int dir) {
        for (uint i = 0; i < want.length(); i++) {
            if (want[i].substr(0, 4) != "eye_") continue;
            if (want[i] != "eye_open" && want[i] != "eye_left"
                && want[i] != "eye_right" && want[i] != "eye_down") return;
            Show(want[i], false);
            string n = dir < 0 ? "eye_left" : (dir == 1 ? "eye_right"
                     : (dir == 2 ? "eye_down" : "eye_open"));
            want[i] = n;
            Show(n, true);
            return;
        }
    }

    // ======================================================================
    void OnUpdate(float dt) {
        if (blinkEvery > 0.0f) {
            blinkT += dt;
            if (!blinking && blinkT >= blinkEvery) {
                blinking = true;  blinkT = 0.0f;
                EyesTo(false);    Show("eye_closed", true);
            } else if (blinking && blinkT >= blinkHold) {
                blinking = false; blinkT = 0.0f;
                Show("eye_closed", false);
                EyesTo(true);
            }
        }
        if (talking && mouthRate > 0.0f) {
            mouthT += dt;
            if (mouthT >= 1.0f / mouthRate) {
                mouthT = 0.0f;
                mouthOpen = !mouthOpen;
                Show("mouth_mid", mouthOpen);
                MouthOfEmote(!mouthOpen);
            }
        }
    }

    void EyesTo(bool on) {
        for (uint i = 0; i < want.length(); i++)
            if (want[i].substr(0, 4) == "eye_") Show(want[i], on);
    }
}
