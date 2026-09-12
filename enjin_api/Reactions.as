// Reactions.as - THE TAG SEAM.
//
// An option in a script carries a TAG, which is just a string. When the option
// fires, the tag comes here, and here is the only place that knows what it
// means. Two things come back out:
//
//   Emote(tag)   what the speaker's face does      -> PortraitRig.Set()
//   Delta(tag)   what it moves, and by how much    -> the game's own scalar
//
// That is the whole seam, and it is what lets one runtime serve two games. Ink
// Ribbon maps "sharpened" onto temper. Another project maps the same tag onto
// affection, or suspicion, or nothing. Neither game edits this file: they each
// ship a Reaction data asset (data/schemas/reaction.enjschema) and name it.
//
//   tags      "verbatim;softened;sharpened;cut"
//   emotes    "neutral;worried;angry;sad"
//   statName  "temper"
//   deltas    "0;-1;1;-1"
//
// Authored in the editor's Data Assets panel. No build script, no recompile.
//
// USE. Not a behavior - make one and load it:
//   Reactions r;
//   r.Load("reactions/ink_ribbon");
//   rig.Set(r.Emote(tag));
//   temper += r.Delta(tag);

class Reactions {
    array<string> tag;
    array<string> emote;
    array<float>  delta;
    string stat = "";
    string source = "";

    // Returns false when the asset is missing, and the table is then empty:
    // every Emote() answers "neutral" and every Delta() answers 0, so a scene
    // with no reaction asset still plays. It just plays flat.
    bool Load(const string &in asset) {
        tag.resize(0); emote.resize(0); delta.resize(0);
        source = asset;
        if (!DataAsset_Load(asset)) return false;

        tag  = Split(DataAsset_GetString(asset, "tags"), ";");
        stat = DataAsset_GetString(asset, "statName");
        array<string> em = Split(DataAsset_GetString(asset, "emotes"), ";");
        array<string> dl = Split(DataAsset_GetString(asset, "deltas"), ";");
        for (uint i = 0; i < tag.length(); i++) {
            emote.insertLast(i < em.length() ? em[i] : "neutral");
            delta.insertLast(i < dl.length() ? Num(dl[i]) : 0.0f);
        }
        return tag.length() > 0;
    }

    // Private, not global. Dictation.as already has its own Split as a class
    // method, and a shared file must never introduce a global that shadows or
    // collides with a host script's member. enjin_api/StrUtil.as has these as
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

    int RxDigit(const string &in c) {
        string digits = "0123456789";
        for (uint i = 0; i < 10; i++) if (digits.substr(i, 1) == c) return int(i);
        return -1;
    }

    // One decimal point, everything after it fractional. Deltas in a reaction
    // table are small and hand-authored, so this is enough. parseFloat does
    // exist here, but this skips stray separators and cannot throw on a cell an
    // author left half-typed, which is the failure that actually happens.
    float ToFloat(const string &in s) {
        float v = 0.0f, scale = 0.0f;
        bool neg = false, any = false;
        for (uint i = 0; i < s.length(); i++) {
            string c = s.substr(i, 1);
            if (i == 0 && c == "-") { neg = true; continue; }
            if (c == "." && scale == 0.0f) { scale = 1.0f; continue; }
            int d = RxDigit(c);
            if (d < 0) continue;
            any = true;
            if (scale == 0.0f) { v = v * 10.0f + float(d); }
            else { scale = scale * 0.1f; v = v + float(d) * scale; }
        }
        if (!any) return 0.0f;
        return neg ? -v : v;
    }

    int Index(const string &in t) {
        for (uint i = 0; i < tag.length(); i++)
            if (tag[i] == t) return int(i);
        return -1;
    }

    string Emote(const string &in t) {
        int i = Index(t);
        return i < 0 ? "neutral" : emote[i];
    }

    float Delta(const string &in t) {
        int i = Index(t);
        return i < 0 ? 0.0f : delta[i];
    }

    string StatName() { return stat; }
    bool Known(const string &in t) { return Index(t) >= 0; }

    // A tag a script uses that the table has never heard of is the failure mode
    // that costs an afternoon, because nothing errors: the face just never
    // moves. Hand this every tag the script can fire and it names the gaps.
    string Unknown(const array<string> &in used) {
        string s = "";
        for (uint i = 0; i < used.length(); i++) {
            if (used[i] == "" || Known(used[i])) continue;
            if (s.findFirst(used[i]) >= 0) continue;
            s += (s == "" ? "" : ", ") + used[i];
        }
        return s;
    }

    float Num(const string &in s) {
        // parseFloat on a stray empty cell gives 0, which is the right answer
        // for a tag that moves nothing.
        return s == "" ? 0.0f : ToFloat(s);
    }
}
