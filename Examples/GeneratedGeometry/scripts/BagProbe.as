// Pulls from all five RandomBag modes at once so the difference between them is
// visible in one console stream rather than described in a doc.
//
// The bag system itself always worked, but RandomBagSystem::ResetAll was only
// called by the editor. An exported game or a web build inherited whatever draw
// order the scene happened to be saved mid-sequence with, and a "shuffled"
// seven-bag came up in the same partial order every launch.
class BagProbe : TegeBehavior {
    array<uint64> bags;
    array<string> labels;
    float autoTimer = 0.0f;

    void OnStart() {
        Add("Bag_Uniform",   "uniform  ");
        Add("Bag_Weighted",  "weighted ");
        Add("Bag_NoReplace", "noReplace");
        Add("Bag_Deck",      "deck     ");
        Add("Bag_Markov",    "markov   ");
        Debug_Log("BagProbe: " + bags.length() + " bags. D = draw once, A = draw ten.");
        DrawAll();
    }

    void Add(const string &in name, const string &in label) {
        uint64 e = Scene_FindEntity(name);
        if (e == 0) {
            Debug_Log("BagProbe: missing bag entity '" + name + "'");
            return;
        }
        bags.insertLast(e);
        labels.insertLast(label);
    }

    void DrawAll() {
        string line = "draw:";
        for (uint i = 0; i < bags.length(); i++) {
            line += "  " + labels[i] + "=" + RandomBag_Draw(bags[i]);
        }
        Debug_Log(line);
    }

    void OnUpdate(float dt) {
        if (Input_GetKeyDown(Key::D)) DrawAll();
        if (Input_GetKeyDown(Key::A)) {
            for (int i = 0; i < 10; i++) DrawAll();
        }

        // Draw on a timer too, so a headless golden run still produces a
        // sequence in the log without anyone pressing a key.
        autoTimer += dt;
        if (autoTimer >= 1.5f) {
            autoTimer = 0.0f;
            DrawAll();
        }
    }
}
