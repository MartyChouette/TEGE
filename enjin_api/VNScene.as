// VNScene.as - the generic visual-novel / dating-sim beat player.
//
// SHARED. Nothing in here is about any one game. It reads a Conversation data
// asset, walks its beats, moves the speaker's face, offers you an answer where
// the author put one, and moves that character's affinity by whatever the
// reaction table says the tag is worth.
//
// THE WHOLE POINT IS THAT IT KNOWS NOTHING. A choice carries a bare tag string.
// This script hands the tag to Reactions.as and does what it is told. Swap the
// reaction asset and "tease" means something else, with no code change.
//
//   SPACE / ENTER / CLICK   advance a beat, or finish the line early
//   1 / 2 / 3               answer, where the beat has a choice
//
// WHAT THE SCENE MUST CONTAIN (build_vn_template.py makes all of it):
//   VN                  this script
//   Nameplate           text, who is talking
//   Line                text, what they say (revealed a character at a time)
//   Prompt              text, the hint line under the box
//   Opt0 Opt1 Opt2      text, the answers
//   PortraitLeft/Right  a PortraitRig each, layerPrefix "L_" and "R_"
//   DimLeft/DimRight    a dark quad over the bust that is not talking
//   MeterFillLeft/Right a quad whose X scale is that character's affinity
//   MeterNameLeft/Right text
//   BeatCount           text
//
// A missing entity is skipped, never an error, so the scene can be cut down.
#include "TegeBehavior.as"
#include "Reactions.as"

// Affinity survives a scene load through the meta store, so a template scene is
// a real starting point for a game with more than one conversation in it.
const string VN_AFFINITY = "vn.affinity.";

class VNScene : TegeBehavior {
    [Property] string conversation = "";   // a Conversation data asset

    // Used only when `conversation` is empty or missing, so the scene still
    // plays something rather than sitting blank.
    [Property] string fallbackLine = "No conversation asset loaded.";

    [Property] float revealRate = 42.0f;   // characters per second
    [Property] float meterWidth = 2.6f;    // world width of a full affinity bar
    [Property] int   startAffinity = 40;
    [Property] int   maxAffinity   = 100;

    // Affinity is carried between scenes through the meta store, which is the
    // point of it. A DEMO wants the opposite: press play twice and the meters
    // should not have crept up from last time. The shipped template sets this
    // true; a real game leaves it false and the relationship accumulates.
    [Property] int   resetOnStart = 0;

    // Auto-play. Seconds per beat; 0 means you play it by hand. Above 0 it
    // walks itself taking autoPick at every choice, which is how the template
    // gets captured without a hand on the keyboard.
    [Property] float autoBeat = 0.0f;
    [Property] int   autoPick = 1;

    Reactions rx;

    // --- the conversation, parsed ----------------------------------------
    array<string> castIds, castNames;
    array<string> beatWho, beatEmote, beatLine;
    array<string> beatChoice;            // "" or a choice-group index
    array<string> choiceWho, choiceText, choiceTags, choiceReply;
    array<int>    affinity;

    // --- entities ---------------------------------------------------------
    uint64 nameplate = 0, lineEnt = 0, prompt = 0, beatCount = 0, closingEnt = 0;
    array<uint64> optEnt;
    array<uint64> dimEnt, fillEnt, meterName, meterVal;

    // --- state ------------------------------------------------------------
    int  beat = 0;
    int  mode = 0;              // 0 revealing, 1 waiting, 2 choosing, 3 done
    float shown = 0.0f;
    int  lineLen = 0;
    int  asking = -1;           // choice group being offered
    string pendingReply = "";   // a chosen option's answering line
    float autoT = 0.0f;

    // ======================================================================
    void OnStart() {
        nameplate  = Scene_FindEntity("Nameplate");
        lineEnt    = Scene_FindEntity("Line");
        prompt     = Scene_FindEntity("Prompt");
        beatCount  = Scene_FindEntity("BeatCount");
        closingEnt = Scene_FindEntity("Closing");
        for (int i = 0; i < 3; i++) optEnt.insertLast(Scene_FindEntity("Opt" + i));

        dimEnt.insertLast(Scene_FindEntity("DimLeft"));
        dimEnt.insertLast(Scene_FindEntity("DimRight"));
        fillEnt.insertLast(Scene_FindEntity("MeterFillLeft"));
        fillEnt.insertLast(Scene_FindEntity("MeterFillRight"));
        meterName.insertLast(Scene_FindEntity("MeterNameLeft"));
        meterName.insertLast(Scene_FindEntity("MeterNameRight"));
        meterVal.insertLast(Scene_FindEntity("MeterValLeft"));
        meterVal.insertLast(Scene_FindEntity("MeterValRight"));

        Load();
        rx.Load(DataAsset_GetString(conversation, "reactions"));

        string gaps = rx.Unknown(AllTags());
        if (gaps != "")
            Debug_Log("VNScene: reaction table has no entry for: " + gaps);

        for (uint i = 0; i < castIds.length(); i++) {
            // Carried across scenes, so a second conversation continues where
            // the first left off rather than restarting the relationship.
            affinity.insertLast(resetOnStart != 0 ? startAffinity
                                : Meta_GetInt(VN_AFFINITY + castIds[i], startAffinity));
            Set(meterName[i], i < castNames.length() ? castNames[i] : castIds[i]);
        }
        DrawMeters();
        Set(closingEnt, "");
        Beat();
    }

    void Load() {
        if (conversation != "" && DataAsset_Load(conversation)) {
            castIds     = Split(DataAsset_GetString(conversation, "castIds"), ";");
            castNames   = Split(DataAsset_GetString(conversation, "castNames"), ";");
            beatWho     = Split(DataAsset_GetString(conversation, "beatWho"), "|");
            beatEmote   = Split(DataAsset_GetString(conversation, "beatEmote"), "|");
            beatLine    = Split(DataAsset_GetString(conversation, "beatLine"), "|");
            beatChoice  = Split(DataAsset_GetString(conversation, "beatChoice"), "|");
            choiceWho   = Split(DataAsset_GetString(conversation, "choiceWho"), "|");
            choiceText  = Split(DataAsset_GetString(conversation, "choiceText"), "|");
            choiceTags  = Split(DataAsset_GetString(conversation, "choiceTags"), "|");
            choiceReply = Split(DataAsset_GetString(conversation, "choiceReply"), "|");
            if (beatLine.length() > 0 && beatLine[0] != "") return;
        }
        // Nothing loaded. Say so on the page rather than showing a blank box,
        // because an empty VN scene and a broken one look identical otherwise.
        Debug_Log("VNScene: no conversation '" + conversation + "'");
        castIds.insertLast("left");   castNames.insertLast("");
        beatWho.insertLast("left");   beatEmote.insertLast("worried");
        beatLine.insertLast(fallbackLine);
        beatChoice.insertLast("");
    }

    array<string> AllTags() {
        array<string> all;
        for (uint i = 0; i < choiceTags.length(); i++) {
            array<string> t = Split(choiceTags[i], ";");
            for (uint j = 0; j < t.length(); j++) all.insertLast(t[j]);
        }
        return all;
    }

    // ======================================================================
    // Start the current beat: face, nameplate, dimming, and the line at zero
    // characters revealed.
    void Beat() {
        if (beat >= int(beatLine.length())) { Finish(); return; }

        string who = beat < int(beatWho.length()) ? beatWho[beat] : "";
        string emo = beat < int(beatEmote.length()) ? beatEmote[beat] : "neutral";

        FireEmote(who, emo);
        Speak(who, true);
        Set(nameplate, NameOf(who));
        Set(beatCount, "" + (beat + 1) + " / " + beatLine.length());

        // The one who is NOT talking goes dark. This is the oldest trick in the
        // form and it does more for readability than any amount of animation.
        for (uint i = 0; i < castIds.length() && i < dimEnt.length(); i++)
            Show(dimEnt[i], castIds[i] != who);

        Text_SetContent(lineEnt, beatLine[beat]);
        lineLen = Text_Length(lineEnt);
        Text_RevealTo(lineEnt, 0);
        shown = 0.0f;
        mode = 0;
        HideOptions();
        Set(prompt, "");
    }

    void Finish() {
        mode = 3;
        Speak("", false);
        for (uint i = 0; i < dimEnt.length(); i++) Show(dimEnt[i], false);
        Set(nameplate, "");
        Set(lineEnt, "");
        Set(prompt, "");
        HideOptions();

        string closing = DataAsset_GetString(conversation, "closing");
        Set(closingEnt, closing != "" ? closing : "The evening ends.");
        for (uint i = 0; i < castIds.length(); i++)
            Meta_SetInt(VN_AFFINITY + castIds[i], affinity[i]);
        Meta_Save();

        string next = DataAsset_GetString(conversation, "nextScene");
        if (next != "") Scene_LoadScene(next);
    }

    // ======================================================================
    void OnUpdate(float dt) {
        if (mode == 3) return;

        if (mode == 0) {
            shown += revealRate * dt;
            if (shown >= float(lineLen)) { shown = float(lineLen); Ready(); }
            Text_RevealTo(lineEnt, int(shown));
            if (Advanced()) { shown = float(lineLen); Text_RevealTo(lineEnt, lineLen); Ready(); }
            return;
        }

        if (autoBeat > 0.0f) {
            autoT += dt;
            if (autoT < autoBeat) return;
            autoT = 0.0f;
            if (mode == 2) Answer(autoPick - 1); else Next();
            return;
        }

        if (mode == 2) {
            if (Input_GetKeyDown(Key::Num1)) Answer(0);
            else if (Input_GetKeyDown(Key::Num2)) Answer(1);
            else if (Input_GetKeyDown(Key::Num3)) Answer(2);
            return;
        }
        if (Advanced()) Next();
    }

    // The line has finished arriving. Either there is an answer to give here,
    // or you are just waiting to hear the next one.
    void Ready() {
        Speak("", false);
        asking = ChoiceAt(beat);
        if (asking >= 0) {
            mode = 2;
            ShowOptions(asking);
            Set(prompt, "1 / 2 / 3 to answer");
        } else {
            mode = 1;
            Set(prompt, "space to continue");
        }
    }

    void Next() {
        // An answer that came with its own reply line plays as its own beat,
        // spoken by whoever the choice was aimed at.
        if (pendingReply != "") {
            string who = asking >= 0 && asking < int(choiceWho.length())
                       ? choiceWho[asking] : "";
            Set(nameplate, NameOf(who));
            Text_SetContent(lineEnt, pendingReply);
            lineLen = Text_Length(lineEnt);
            Text_RevealTo(lineEnt, 0);
            shown = 0.0f;
            mode = 0;
            pendingReply = "";
            asking = -1;
            Speak(who, true);
            return;
        }
        beat++;
        Beat();
    }

    // ======================================================================
    // THE SEAM. The option carries a tag; the table turns it into a face and a
    // number. This script never learns what "warm" or "cool" mean.
    void Answer(int opt) {
        if (asking < 0 || opt < 0) return;
        array<string> tags = Split(Get(choiceTags, asking), ";");
        if (opt >= int(tags.length())) return;

        string tag = tags[opt];
        string who = Get(choiceWho, asking);
        int    idx = IndexOfCast(who);

        // SPEND THE CHOICE. The answering line plays as its own beat WITHOUT
        // advancing `beat`, so without this the beat still carries its choice
        // and Ready() offers the same question again the moment the reply
        // finishes. It reads as one question you can never stop answering, and
        // the meter climbs to full on a single line.
        beatChoice[beat] = "";

        if (idx >= 0) {
            affinity[idx] += int(rx.Delta(tag));
            if (affinity[idx] < 0) affinity[idx] = 0;
            if (affinity[idx] > maxAffinity) affinity[idx] = maxAffinity;
            DrawMeters();
        }
        FireEmote(who, rx.Emote(tag));
        Debug_Log("VNScene: tag '" + tag + "' -> " + who + " affinity "
                  + (idx >= 0 ? "" + affinity[idx] : "?") + " face " + rx.Emote(tag));

        array<string> replies = Split(Get(choiceReply, asking), ";");
        pendingReply = opt < int(replies.length()) ? replies[opt] : "";

        HideOptions();
        Set(prompt, "space to continue");
        mode = 1;
        if (autoBeat > 0.0f) autoT = 0.0f;
    }

    int ChoiceAt(int b) {
        if (b >= int(beatChoice.length())) return -1;
        if (beatChoice[b] == "") return -1;
        int g = ToInt(beatChoice[b]);
        return g >= 0 && g < int(choiceText.length()) ? g : -1;
    }

    void ShowOptions(int group) {
        array<string> opts = Split(Get(choiceText, group), ";");
        for (uint i = 0; i < optEnt.length(); i++)
            Set(optEnt[i], i < opts.length() ? ("" + (i + 1) + "   " + opts[i]) : "");
    }

    void HideOptions() {
        for (uint i = 0; i < optEnt.length(); i++) Set(optEnt[i], "");
    }

    void DrawMeters() {
        for (uint i = 0; i < affinity.length() && i < fillEnt.length(); i++) {
            if (fillEnt[i] == 0) continue;
            float f = maxAffinity > 0 ? float(affinity[i]) / float(maxAffinity) : 0.0f;
            Vector3 s = Entity_GetScale(fillEnt[i]);
            Vector3 p = Entity_GetPosition(fillEnt[i]);
            // The bar grows from its left edge, so the centre has to move with
            // the width. Anchoring it at the middle makes it grow both ways.
            float w = meterWidth * f;
            float left = p.x - s.x * 0.5f;
            Entity_SetScale(fillEnt[i], Vector3(w > 0.001f ? w : 0.001f, s.y, s.z));
            Entity_SetPosition(fillEnt[i], Vector3(left + w * 0.5f, p.y, p.z));
            if (i < meterVal.length()) Set(meterVal[i], "" + affinity[i]);
        }
    }

    // ======================================================================
    // Helpers. Every one tolerates a missing entity or a short array, because a
    // template gets cut down and should degrade instead of failing.
    string Get(const array<string> &in a, int i) {
        return i >= 0 && i < int(a.length()) ? a[i] : "";
    }

    int IndexOfCast(const string &in id) {
        for (uint i = 0; i < castIds.length(); i++)
            if (castIds[i] == id) return int(i);
        return -1;
    }

    string NameOf(const string &in id) {
        int i = IndexOfCast(id);
        return i >= 0 && i < int(castNames.length()) ? castNames[i] : id;
    }

    void FireEmote(const string &in who, const string &in emote) {
        if (who == "") return;
        EventData@ d = EventData();
        d.SetString("who", who);
        d.SetString("emote", emote);
        Events_Send("portrait_emote", d);
    }

    void Speak(const string &in who, bool on) {
        EventData@ d = EventData();
        d.SetString("who", who);
        d.SetInt("on", on ? 1 : 0);
        Events_Send("portrait_speak", d);
    }

    bool Advanced() {
        if (autoBeat > 0.0f) return false;
        return Input_GetKeyDown(Key::Space) || Input_GetKeyDown(Key::Enter)
            || Input_GetMouseButtonDown(0);
    }

    void Set(uint64 e, const string &in s) {
        if (e != 0) Text_SetContent(e, s);
    }

    void Show(uint64 e, bool on) {
        if (e != 0) Entity_SetVisible(e, on);
    }

    // Private, not global: a shared file must not introduce a global that
    // collides with a host script's member. See enjin_api/StrUtil.as.
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

    int VnDigit(const string &in c) {
        string digits = "0123456789";
        for (uint i = 0; i < 10; i++) if (digits.substr(i, 1) == c) return int(i);
        return -1;
    }

    int ToInt(const string &in s) {
        int v = 0; bool neg = false, any = false;
        for (uint i = 0; i < s.length(); i++) {
            string c = s.substr(i, 1);
            if (i == 0 && c == "-") { neg = true; continue; }
            int d = VnDigit(c);
            if (d < 0) continue;
            v = v * 10 + d; any = true;
        }
        if (!any) return 0;
        return neg ? -v : v;
    }
}
