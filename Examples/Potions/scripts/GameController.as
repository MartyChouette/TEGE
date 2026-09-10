// GameController.as — complete Potions game implementation.
// Multi-town progression (~10 towns, 30 battles), Town Shops, Alchemist Home,
// MP cost system, Soft Type Chart color clash, healing items, smooth transitions.
#include "TegeBehavior.as"
#include "PotionData.as"

const float TALK_RANGE = 1.8f;
const float DOOR_RANGE = 2.0f;
const float SHOP_RANGE = 2.2f;
const float GATE_RANGE = 2.2f;
const float CAULDRON_RANGE = 2.2f;
const float EXIT_RANGE = 2.0f;

const int MODE_TOWN = 0, MODE_BATTLE = 1, MODE_HOME = 2, MODE_SHOP = 3, MODE_VICTORY = 4;
const int TR_NONE = 0, TR_OUT = 1, TR_IN = 2;
const int BST_FIGHT = 0, BST_FOOD = 1, BST_STATS = 2;

const int START_HP = 24, START_MP = 15, BASE_COST = 3;
const float Z_BASE = 6.0f, Z_SCALE = 0.1f;
const float TR_OUT_TIME = 0.28f, TR_IN_TIME = 0.34f;
const int WARMUP_FRAMES = 4;

float DepthForY(float y) { return Z_BASE - y * Z_SCALE; }

class GameController : TegeBehavior {
    array<string> towns = {
        "Karada", "Sofida", "Frazeail", "Tobito", "Prala",
        "Gornot", "Qaytail", "Lesnod", "Rezaal", "Wrostlane"
    };
    int townIndex = 0;

    array<uint64> npcEnts;
    array<bool>   beaten;

    array<uint64> townEnts, homeEnts, battleEnts, enemyEnts;
    uint64 hud, fade, homeDoor, shopDoor, shopKeeper, nextGate, cauldron, exitDoor;
    uint64 eFill, pFill, throwP, throwE;
    Vector3 eFull, pFull;
    float   eFw, pFw;

    int hp = START_HP, maxHP = START_HP;
    int mp = START_MP, maxMP = START_MP;
    int coins = 12;
    array<int> bag = { 3, 1, 3, 1, 3, 1 };      // Red, Orange, Yellow, Green, Blue, Purple
    array<int> items = { 1, 1, 0 };             // Berries, Salad, Baguette

    int mode = MODE_TOWN;
    int trans = TR_NONE, pendingMode = MODE_TOWN, pendingEnemy = -1;
    float transT = 0.0f;
    Vector3 townReturn = Vector3(0.0f, -2.0f, 0.0f);

    bool warmup = true;
    int  warmN = 0;

    uint64 nearNpc = 0;
    bool   nearHome = false, nearShop = false, nearGate = false;
    string lastResult = "";

    int bstate = BST_FIGHT, enemyIdx = -1, enemyFav = COL_RED, enemyHP = 20, maxEnemyHP = 20;
    string enemyName = "";
    int dealt = 0, taken = 0, turns = 0;

    bool atCauldron = false, atExit = false, mixing = false;

    // ---------------------------------------------------------------- setup
    void OnStart() {
        hud        = Scene_FindEntity("GameHUD");
        fade       = Scene_FindEntity("Fade");
        homeDoor   = Scene_FindEntity("HomeDoor");
        shopDoor   = Scene_FindEntity("ShopDoor");
        shopKeeper = Scene_FindEntity("ShopKeeper");
        nextGate   = Scene_FindEntity("NextTownGate");
        cauldron   = Scene_FindEntity("Cauldron");
        exitDoor   = Scene_FindEntity("ExitDoor");
        eFill      = Scene_FindEntity("EnemyHPFill");
        pFill      = Scene_FindEntity("PlayerHPFill");
        throwP     = Scene_FindEntity("ThrowPlayer");
        throwE     = Scene_FindEntity("ThrowEnemy");
        eFull = Entity_GetPosition(eFill); eFw = Entity_GetScale(eFill).x;
        pFull = Entity_GetPosition(pFill); pFw = Entity_GetScale(pFill).x;

        for (int i = 0; i < 3; i++) {
            npcEnts.insertLast(Scene_FindEntity("NPC_" + i));
            beaten.insertLast(false);
            enemyEnts.insertLast(Scene_FindEntity("BattleEnemy" + i));
        }

        townEnts.insertLast(Scene_FindEntity("Ground"));
        for (uint i = 0; i < npcEnts.length(); i++) townEnts.insertLast(npcEnts[i]);
        townEnts.insertLast(homeDoor);
        townEnts.insertLast(shopDoor);
        townEnts.insertLast(shopKeeper);
        townEnts.insertLast(nextGate);

        homeEnts.insertLast(Scene_FindEntity("FloorHome"));
        homeEnts.insertLast(cauldron);
        homeEnts.insertLast(exitDoor);

        battleEnts.insertLast(Scene_FindEntity("BattleBG"));
        battleEnts.insertLast(Scene_FindEntity("BattlePlayer"));
        battleEnts.insertLast(Scene_FindEntity("EnemyHPBg"));
        battleEnts.insertLast(eFill);
        battleEnts.insertLast(Scene_FindEntity("PlayerHPBg"));
        battleEnts.insertLast(pFill);

        warmup = true; warmN = 0;
    }

    void SV(uint64 e, bool v) { if (e != 0) Entity_SetVisible(e, v); }
    void SetHUD(const string &in s) { if (hud != 0) HUD_SetText(hud, s); }

    // Town rival properties based on current town and rival index (0, 1, 2)
    string GetRivalName(int t, int r) {
        if (t == 0) { if (r==0) return "Bram"; if (r==1) return "Odette"; return "Milo"; }
        if (t == 1) { if (r==0) return "Ivy"; if (r==1) return "Flynn"; return "Cleo"; }
        if (t == 2) { if (r==0) return "Amber"; if (r==1) return "Cobalt"; return "Topaz"; }
        if (t == 3) { if (r==0) return "Jade"; if (r==1) return "Russet"; return "Amethyst"; }
        if (t == 4) { if (r==0) return "Ruby"; if (r==1) return "Sapphire"; return "Emerald"; }
        if (t == 5) { if (r==0) return "Jasper"; if (r==1) return "Hazel"; return "Violet"; }
        if (t == 6) { if (r==0) return "Carmine"; if (r==1) return "Indigo"; return "Forest"; }
        if (t == 7) { if (r==0) return "Flame"; if (r==1) return "Aureolin"; return "Lavender"; }
        if (t == 8) { if (r==0) return "Crimson"; if (r==1) return "Azure"; return "Veridian"; }
        if (r==0) return "Grand Master Zenith";
        if (r==1) return "Alchemist Vane";
        return "Grand Champion Sol";
    }

    int GetRivalFav(int t, int r) {
        if (t == 0) { if (r==0) return COL_RED; if (r==1) return COL_BLUE; return COL_YELLOW; }
        if (t == 1) { if (r==0) return COL_GREEN; if (r==1) return COL_ORANGE; return COL_PURPLE; }
        if (t == 2) { if (r==0) return COL_RED; if (r==1) return COL_BLUE; return COL_YELLOW; }
        if (t == 3) { if (r==0) return COL_GREEN; if (r==1) return COL_ORANGE; return COL_PURPLE; }
        if (t == 4) { if (r==0) return COL_RED; if (r==1) return COL_BLUE; return COL_GREEN; }
        if (t == 5) { if (r==0) return COL_ORANGE; if (r==1) return COL_YELLOW; return COL_PURPLE; }
        if (t == 6) { if (r==0) return COL_RED; if (r==1) return COL_BLUE; return COL_GREEN; }
        if (t == 7) { if (r==0) return COL_ORANGE; if (r==1) return COL_YELLOW; return COL_PURPLE; }
        if (t == 8) { if (r==0) return COL_RED; if (r==1) return COL_BLUE; return COL_GREEN; }
        if (r==0) return COL_PURPLE; if (r==1) return COL_ORANGE; return COL_GREEN;
    }

    int GetRivalMaxHP(int t, int r) {
        if (t == 9) return 70 + r * 10;
        return 20 + t * 4 + r * 3;
    }

    bool AllBeaten() {
        for (uint i = 0; i < beaten.length(); i++) if (!beaten[i]) return false;
        return true;
    }

    void ShowOnly(int m) {
        for (uint i = 0; i < townEnts.length(); i++) SV(townEnts[i], m == MODE_TOWN);
        for (uint i = 0; i < homeEnts.length(); i++) SV(homeEnts[i], m == MODE_HOME);
        for (uint i = 0; i < battleEnts.length(); i++) SV(battleEnts[i], m == MODE_BATTLE);
        for (uint i = 0; i < enemyEnts.length(); i++) SV(enemyEnts[i], false);
        SV(_entityId, m == MODE_TOWN || m == MODE_HOME || m == MODE_SHOP);
        SV(throwP, false);
        SV(throwE, false);
        if (m == MODE_TOWN) {
            for (uint i = 0; i < npcEnts.length(); i++) if (beaten[i]) SV(npcEnts[i], false);
            SV(nextGate, AllBeaten());
        }
    }

    // ------------------------------------------------------- transition FSM
    void Begin(int target, int enemy) {
        if (trans != TR_NONE) return;
        pendingMode = target;
        pendingEnemy = enemy;
        trans = TR_OUT;
        transT = 0.0f;
        if (fade != 0) {
            Entity_SetVisible(fade, true);
            Entity_SetRotation(fade, Vector3(0.0f, 0.0f, 0.0f));
            Tween_Opacity(fade, 1.0f, TR_OUT_TIME, EASE_IN_OUT_CUBIC);
            Tween_Rotation(fade, Vector3(0.0f, 0.0f, 220.0f), TR_OUT_TIME + TR_IN_TIME, EASE_IN_OUT_CUBIC);
        }
    }

    void OnUpdate(float dt) {
        if (warmup) {
            warmN++;
            if (warmN >= WARMUP_FRAMES) {
                warmup = false;
                mode = MODE_TOWN;
                ShowOnly(MODE_TOWN);
                MovePlayer(0.0f, -2.0f);
                ShowTown();
                trans = TR_IN; transT = 0.0f;
                if (fade != 0) Tween_Opacity(fade, 0.0f, TR_IN_TIME, EASE_OUT_CUBIC);
            }
            return;
        }
        if (trans != TR_NONE) { UpdateTransition(dt); return; }
        if (mode == MODE_TOWN) UpdateTown(dt);
        else if (mode == MODE_BATTLE) UpdateBattle(dt);
        else if (mode == MODE_HOME) UpdateHome(dt);
        else if (mode == MODE_SHOP) UpdateShop(dt);
        else if (mode == MODE_VICTORY) UpdateVictory(dt);
    }

    void UpdateTransition(float dt) {
        transT += dt;
        if (transT > 1.0f) { trans = TR_NONE; if (fade != 0) Entity_SetVisible(fade, false); return; }
        if (trans == TR_OUT) {
            if (transT >= TR_OUT_TIME) {
                ApplyMode(pendingMode);
                trans = TR_IN; transT = 0.0f;
                if (fade != 0) Tween_Opacity(fade, 0.0f, TR_IN_TIME, EASE_OUT_CUBIC);
            }
        } else {
            if (transT >= TR_IN_TIME) { trans = TR_NONE; if (fade != 0) Entity_SetVisible(fade, false); }
        }
    }


    // ==================================================== TOUCH BUTTONS ====
    // Every verb in this game lives on the number row, which on a phone means
    // it does not exist: the touch overlay is generated from BINDINGS, and a
    // key nothing claims gets no button. That left the shop unusable and the
    // game unfinishable on a touchscreen.
    //
    // Each screen publishes its own set, because the same key means different
    // things in the shop, at the cauldron and mid-battle. Labels are capped at
    // 7 characters by the overlay.
    void TouchAdd(const string &in label, int key) {
        // Slot geometry is chosen by the engine in add order (bottom-right
        // outward), so callers pick only order and label.
        Touch_AddButton(label, key, 0.0f, 0.0f, 0.0f);
    }

    void TouchTown() {
        Touch_ClearButtons();
        TouchAdd("ENTER", Key::E);
    }

    void TouchShop() {
        Touch_ClearButtons();
        TouchAdd("RED",   Key::Num1);
        TouchAdd("YELLOW", Key::Num2);
        TouchAdd("BLUE",  Key::Num3);
        TouchAdd("BERRY", Key::Num4);
        TouchAdd("SALAD", Key::Num5);
        TouchAdd("BREAD", Key::Num6);
        TouchAdd("LEAVE", Key::E);
    }

    void TouchHome() {
        Touch_ClearButtons();
        TouchAdd("USE", Key::E);
    }

    void TouchMix() {
        Touch_ClearButtons();
        TouchAdd("ORANGE", Key::Num1);
        TouchAdd("GREEN",  Key::Num2);
        TouchAdd("PURPLE", Key::Num3);
        TouchAdd("REST",   Key::Num4);
        TouchAdd("BUY R",  Key::Num5);
        TouchAdd("BUY Y",  Key::Num6);
        TouchAdd("BUY B",  Key::Num7);
        TouchAdd("BACK",   Key::E);
    }

    void TouchBattle() {
        Touch_ClearButtons();
        TouchAdd("RED",    Key::Num1);
        TouchAdd("YELLOW", Key::Num2);
        TouchAdd("BLUE",   Key::Num3);
        TouchAdd("FOOD",   Key::Num7);
    }

    void TouchFood() {
        Touch_ClearButtons();
        TouchAdd("BERRY", Key::Num1);
        TouchAdd("SALAD", Key::Num2);
        TouchAdd("BREAD", Key::Num3);
        TouchAdd("BACK",  Key::E);
    }

    void TouchContinue() {
        Touch_ClearButtons();
        TouchAdd("OK", Key::E);
    }

    void ApplyMode(int m) {
        mode = m;
        ShowOnly(m);
        if (m == MODE_TOWN) { MovePlayer(townReturn.x, townReturn.y); ShowTown(); }
        else if (m == MODE_HOME) { MovePlayer(-8.0f, -3.0f); mixing = false; ShowHome(""); }
        else if (m == MODE_SHOP) { MovePlayer(1.0f, -3.0f); ShowShop(""); }
        else if (m == MODE_BATTLE) StartBattle(pendingEnemy);
        else ShowVictory();
    }

    void MovePlayer(float x, float y) { Entity_SetPosition(_entityId, Vector3(x, y, DepthForY(y))); }

    // ============================================================ TOWN =====
    void ShowTown() {
        TouchTown();
        int remain = 0;
        for (uint i = 0; i < beaten.length(); i++) if (!beaten[i]) remain++;

        string s = "== Town " + (townIndex + 1) + "/" + towns.length() + ": " + towns[townIndex % int(towns.length())]
                 + " ==   Coins: " + coins + " | HP: " + hp + "/" + maxHP + " | MP: " + mp + "/" + maxMP + "\n";
        if (lastResult.length() > 0) s += lastResult + "\n";

        if (remain > 0) {
            s += "Challengers remaining: " + remain + ". Walk up to a rival (press E), or visit Home/Shop.";
        } else {
            s += "* TOWN CLEARED! * The Gate to the Next Town is open! Walk to the Gate (far right) and press E.";
        }
        SetHUD(s);
    }

    void UpdateTown(float dt) {
        Vector3 me = GetPosition();
        MovePlayer(me.x, me.y);

        uint64 best = 0; float bd = TALK_RANGE;
        for (uint i = 0; i < npcEnts.length(); i++) {
            if (npcEnts[i] == 0 || beaten[i]) continue;
            float d = (Entity_GetPosition(npcEnts[i]) - me).Length();
            if (d < bd) { bd = d; best = npcEnts[i]; }
        }

        bool home = (homeDoor != 0) && ((Entity_GetPosition(homeDoor) - me).Length() < DOOR_RANGE);
        bool shopB = ((shopDoor != 0) && ((Entity_GetPosition(shopDoor) - me).Length() < SHOP_RANGE)) ||
                     ((shopKeeper != 0) && ((Entity_GetPosition(shopKeeper) - me).Length() < TALK_RANGE));
        bool gateB = AllBeaten() && (nextGate != 0) && ((Entity_GetPosition(nextGate) - me).Length() < GATE_RANGE);

        if (best != nearNpc || home != nearHome || shopB != nearShop || gateB != nearGate) {
            nearNpc = best; nearHome = home; nearShop = shopB; nearGate = gateB;
            if (nearNpc != 0) {
                int i = TownIndexOf(nearNpc);
                string rName = GetRivalName(townIndex, i);
                int rFav = GetRivalFav(townIndex, i);
                int rHP = GetRivalMaxHP(townIndex, i);
                SetHUD("Challenger " + rName + " (HP " + rHP + ") favors " + ColorName(rFav) + "!\nPress E to battle.");
            } else if (nearHome) {
                SetHUD("Alchemist Workbench & Home.\nPress E to go inside to mix pigments & rest.");
            } else if (nearShop) {
                SetHUD("Town Merchant Shop.\nPress E to buy potion pigments and healing foods.");
            } else if (nearGate) {
                SetHUD("Gate to Next Town.\nPress E to travel to " + NextTownName() + "!");
            } else ShowTown();
        }

        if (Input_GetKeyDown(Key::E)) {
            if (nearNpc != 0) { townReturn = Vector3(me.x, me.y, 0.0f); Begin(MODE_BATTLE, TownIndexOf(nearNpc)); }
            else if (nearHome) { townReturn = Vector3(-8.0f, -2.0f, 0.0f); Begin(MODE_HOME, -1); }
            else if (nearShop) { townReturn = Vector3(1.0f, -2.0f, 0.0f); Begin(MODE_SHOP, -1); }
            else if (nearGate) AdvanceTown();
        }
    }

    string NextTownName() {
        int nxt = townIndex + 1;
        if (nxt < int(towns.length())) return towns[nxt];
        return "Grand Championship";
    }

    void AdvanceTown() {
        townIndex++;
        if (townIndex >= int(towns.length())) {
            Begin(MODE_VICTORY, -1);
            return;
        }
        for (uint i = 0; i < beaten.length(); i++) beaten[i] = false;
        maxHP += 2; hp = maxHP;
        maxMP += 2; mp = maxMP;
        lastResult = "Welcome to " + towns[townIndex] + "! Your Max HP & MP increased (+2)!";
        townReturn = Vector3(-8.0f, -2.0f, 0.0f);
        Begin(MODE_TOWN, -1);
    }

    int TownIndexOf(uint64 e) {
        for (uint i = 0; i < npcEnts.length(); i++) if (npcEnts[i] == e) return int(i);
        return -1;
    }

    // ========================================================== SHOP =====
    void ShowShop(const string &in note) {
        TouchShop();
        string s = "== Town Merchant Shop ==   Coins: " + coins + "\n";
        if (note.length() > 0) s += note + "\n";
        s += "Potions (3 coins): [1] Red (x" + bag[COL_RED] + ")  [2] Yellow (x" + bag[COL_YELLOW] + ")  [3] Blue (x" + bag[COL_BLUE] + ")\n";
        s += "Food Items:  [4] Berries (+5 HP, 4c: x" + items[ITEM_BERRIES] + ")  [5] Salad (+10 HP, 7c: x" + items[ITEM_SALAD]
             + ")  [6] Baguette (Full HP, 12c: x" + items[ITEM_BAGUETTE] + ")\n";
        s += "[E] Exit Shop";
        SetHUD(s);
    }

    void UpdateShop(float dt) {
        if (Input_GetKeyDown(Key::E)) { Begin(MODE_TOWN, -1); return; }
        if (Input_GetKeyDown(Key::Num1)) BuyShopPotion(COL_RED);
        else if (Input_GetKeyDown(Key::Num2)) BuyShopPotion(COL_YELLOW);
        else if (Input_GetKeyDown(Key::Num3)) BuyShopPotion(COL_BLUE);
        else if (Input_GetKeyDown(Key::Num4)) BuyShopItem(ITEM_BERRIES);
        else if (Input_GetKeyDown(Key::Num5)) BuyShopItem(ITEM_SALAD);
        else if (Input_GetKeyDown(Key::Num6)) BuyShopItem(ITEM_BAGUETTE);
    }

    void BuyShopPotion(int c) {
        if (coins < BASE_COST) { ShowShop("Not enough coins for " + ColorName(c) + " potion!"); return; }
        coins -= BASE_COST; bag[c] += 1;
        ShowShop("Bought " + ColorName(c) + " potion! (x" + bag[c] + ")");
    }

    void BuyShopItem(int it) {
        int cost = ItemCost(it);
        if (coins < cost) { ShowShop("Not enough coins for " + ItemName(it) + "!"); return; }
        coins -= cost; items[it] += 1;
        ShowShop("Bought " + ItemName(it) + "! (x" + items[it] + ")");
    }

    // ========================================================== BATTLE =====
    void StartBattle(int i) {
        if (i < 0 || i >= int(npcEnts.length())) i = 0;
        enemyIdx = i;
        enemyName = GetRivalName(townIndex, i);
        enemyFav = GetRivalFav(townIndex, i);
        maxEnemyHP = GetRivalMaxHP(townIndex, i);
        enemyHP = maxEnemyHP;
        bstate = BST_FIGHT;
        dealt = 0; taken = 0; turns = 0;

        for (uint k = 0; k < enemyEnts.length(); k++) SV(enemyEnts[k], int(k) == i);
        SV(throwP, false); SV(throwE, false);
        UpdateBars();
        ShowBattleMenu(enemyName + " challenges you to a potion clash! (Favors " + ColorName(enemyFav) + ")");
    }

    void UpdateBattle(float dt) {
        if (bstate == BST_STATS) {
            if (Input_GetKeyDown(Key::E) || Input_GetKeyDown(Key::Enter)) Begin(MODE_TOWN, -1);
            return;
        }

        if (bstate == BST_FOOD) {
            UpdateFoodMenu();
            return;
        }

        if (Input_GetKeyDown(Key::Num7) || Input_GetKeyDown(Key::H)) {
            bstate = BST_FOOD;
            ShowFoodMenu("");
            return;
        }

        for (int c = 0; c < COL_COUNT; c++) {
            if (Input_GetKeyDown(int(Key::Num1) + c)) {
                int cost = ColorMPCost(c);
                if (bag[c] <= 0) ShowBattleMenu("No " + ColorName(c) + " potions left! Mix or buy more.");
                else if (mp < cost) ShowBattleMenu("Not enough MP! " + ColorName(c) + " costs " + cost + " MP (You have " + mp + ").");
                else ResolveRound(c);
                return;
            }
        }
    }

    void ResolveRound(int myColor) {
        int cost = ColorMPCost(myColor);
        bag[myColor] -= 1; mp -= cost; turns += 1;

        int dOut = PotionDamage(myColor, enemyFav);
        int dIn  = PotionDamage(enemyFav, myColor);
        enemyHP -= dOut; dealt += dOut;
        hp -= dIn; taken += dIn;

        ShowThrow(throwP, myColor);
        ShowThrow(throwE, enemyFav);
        UpdateBars();

        string line = "You threw " + ColorName(myColor) + " " + Eff(myColor, enemyFav)
                    + "-> " + dOut + " dmg.  " + enemyName + "'s "
                    + ColorName(enemyFav) + " dealt " + dIn + " dmg. (-" + cost + " MP)";

        if (enemyHP <= 0) { Finish(true); return; }
        if (hp <= 0) { Finish(false); return; }
        if (BagTotal() <= 0 && FoodTotal() <= 0) { Finish(false); return; }
        ShowBattleMenu(line);
    }

    void ShowFoodMenu(const string &in note) {
        TouchFood();
        string s = "== In-Battle Healing Items ==   HP: " + hp + "/" + maxHP + "\n";
        if (note.length() > 0) s += note + "\n";
        s += "[1] Berries (+5 HP, x" + items[ITEM_BERRIES] + ")  [2] Salad (+10 HP, x" + items[ITEM_SALAD]
             + ")  [3] Baguette (Full HP, x" + items[ITEM_BAGUETTE] + ")\n";
        s += "[E] Back to potion selection";
        SetHUD(s);
    }

    void UpdateFoodMenu() {
        if (Input_GetKeyDown(Key::E)) { bstate = BST_FIGHT; ShowBattleMenu(""); return; }
        if (Input_GetKeyDown(Key::Num1)) UseFoodInBattle(ITEM_BERRIES);
        else if (Input_GetKeyDown(Key::Num2)) UseFoodInBattle(ITEM_SALAD);
        else if (Input_GetKeyDown(Key::Num3)) UseFoodInBattle(ITEM_BAGUETTE);
    }

    void UseFoodInBattle(int it) {
        if (items[it] <= 0) { ShowFoodMenu("No " + ItemName(it) + " remaining!"); return; }
        items[it] -= 1;
        int heal = ItemHeal(it);
        hp += heal; if (hp > maxHP) hp = maxHP;
        bstate = BST_FIGHT;
        UpdateBars();
        ShowBattleMenu("Ate " + ItemName(it) + "! Restored HP to " + hp + "/" + maxHP + ".");
    }

    void ShowThrow(uint64 e, int color) {
        if (e == 0) return;
        Material_SetBaseColor(e, ColorRGB(color));
        Entity_SetVisible(e, true);
    }

    string Eff(int a, int d) {
        float m = TypeMultiplier(a, d);
        if (m >= 2.0f) return "(SUPER EFFECTIVE! 2x) ";
        if (m <= 0.5f) return "(NOT VERY EFFECTIVE 0.5x) ";
        return "";
    }

    int BagTotal() { int t = 0; for (int c = 0; c < COL_COUNT; c++) t += bag[c]; return t; }
    int FoodTotal() { int t = 0; for (int i = 0; i < ITEM_COUNT; i++) t += items[i]; return t; }

    void UpdateBars() {
        SetFill(eFill, eFull, eFw, Frac(enemyHP, maxEnemyHP));
        SetFill(pFill, pFull, pFw, Frac(hp, maxHP));
    }

    float Frac(int v, int mx) {
        if (mx <= 0) return 0.0f;
        float f = float(v) / float(mx);
        return (f < 0.0f) ? 0.0f : ((f > 1.0f) ? 1.0f : f);
    }

    void SetFill(uint64 e, const Vector3 &in full, float fullW, float frac) {
        if (e == 0) return;
        float w = fullW * frac; if (w < 0.02f) w = 0.02f;
        Vector3 s = Entity_GetScale(e); s.x = w; Entity_SetScale(e, s);
        Vector3 p = full; p.x = (full.x - fullW * 0.5f) + w * 0.5f; Entity_SetPosition(e, p);
        Vector3 col = (frac > 0.5f) ? Vector3(0.30f, 0.80f, 0.35f)
                    : (frac > 0.25f) ? Vector3(0.90f, 0.80f, 0.20f)
                                     : Vector3(0.85f, 0.25f, 0.20f);
        Material_SetBaseColor(e, col);
    }

    void ShowBattleMenu(const string &in note) {
        TouchBattle();
        string s = note + "\n\nHP: " + hp + "/" + maxHP + " | MP: " + mp + "/" + maxMP + " | Enemy HP: " + enemyHP + "/" + maxEnemyHP + "\n";
        for (int c = 0; c < COL_COUNT; c++) {
            int cost = ColorMPCost(c);
            s += "[" + (c + 1) + "] " + ColorName(c) + " (x" + bag[c] + ", " + cost + "mp)   ";
        }
        s += "\n[7] Use Food Item (x" + FoodTotal() + ")";
        SetHUD(s);
    }

    void Finish(bool win) {
        int reward = 0;
        if (win) {
            reward = 12 + townIndex * 4 + turns;
            beaten[enemyIdx] = true;
            coins += reward;
            if (hp < 1) hp = 1;
        } else {
            hp = maxHP;
            mp = maxMP;
        }
        SV(throwP, false); SV(throwE, false);
        bstate = BST_STATS;
        // The results screen takes only E. Without this the colour buttons from
        // the fight stay on screen and do nothing.
        TouchContinue();

        string s = win ? ("* VICTORY OVER " + enemyName + "! *") : ("Defeated by " + enemyName + "...");
        s += "\nDealt " + dealt + " dmg   Taken " + taken + " dmg   Rounds: " + turns + "\n";
        s += win ? ("Earned +" + reward + " coins! (Total coins: " + coins + ")\n") : ("You rest and recover full HP & MP.\n");
        s += "Press E to return to town.";
        lastResult = win ? ("You defeated " + enemyName + "!") : (enemyName + " defeated you.");
        SetHUD(s);
    }

    // ============================================================ HOME =====
    void ShowHome(const string &in note) {
        TouchHome();
        string s = "== Alchemist Home ==   HP: " + hp + "/" + maxHP + " | MP: " + mp + "/" + maxMP + " | Coins: " + coins + "\n";
        if (note.length() > 0) s += note + "\n";
        s += "Walk to the Cauldron to mix pigments, or the Door to exit.";
        SetHUD(s);
    }

    void ShowMix(const string &in note) {
        TouchMix();
        string s = "== Alchemy Workbench ==   Coins: " + coins + "\n";
        if (note.length() > 0) s += note + "\n";
        s += "Mix Secondaries: [1] Orange (Red+Yellow)  [2] Green (Yellow+Blue)  [3] Purple (Blue+Red)\n";
        s += "Buy Primaries (3c): [5] Red (x" + bag[COL_RED] + ")  [6] Yellow (x" + bag[COL_YELLOW] + ")  [7] Blue (x" + bag[COL_BLUE] + ")\n";
        s += "[4] Rest at Bed (Restores Full HP & MP)\n[E] Step away";
        SetHUD(s);
    }

    void UpdateHome(float dt) {
        Vector3 me = GetPosition();
        MovePlayer(me.x, me.y);
        bool nc = (cauldron != 0) && ((Entity_GetPosition(cauldron) - me).Length() < CAULDRON_RANGE);
        bool nx = (exitDoor != 0) && ((Entity_GetPosition(exitDoor) - me).Length() < EXIT_RANGE);

        if (mixing) { UpdateMix(); return; }

        if (nc != atCauldron || nx != atExit) {
            atCauldron = nc; atExit = nx;
            if (atCauldron) ShowHome("Press E to use the Workbench & Cauldron.");
            else if (atExit) ShowHome("Press E to head back out to town.");
            else ShowHome("");
        }

        if (Input_GetKeyDown(Key::E)) {
            if (atCauldron) { mixing = true; ShowMix(""); }
            else if (atExit) { townReturn = Vector3(-8.0f, -2.0f, 0.0f); Begin(MODE_TOWN, -1); }
        }
    }

    void UpdateMix() {
        if (Input_GetKeyDown(Key::E)) { mixing = false; ShowHome(""); return; }
        if (Input_GetKeyDown(Key::Num1)) Mix(COL_RED, COL_YELLOW, COL_ORANGE);
        else if (Input_GetKeyDown(Key::Num2)) Mix(COL_YELLOW, COL_BLUE, COL_GREEN);
        else if (Input_GetKeyDown(Key::Num3)) Mix(COL_BLUE, COL_RED, COL_PURPLE);
        else if (Input_GetKeyDown(Key::Num4)) { hp = maxHP; mp = maxMP; ShowMix("Rested at bed! HP and MP fully restored."); }
        else if (Input_GetKeyDown(Key::Num5)) BuyHome(COL_RED);
        else if (Input_GetKeyDown(Key::Num6)) BuyHome(COL_YELLOW);
        else if (Input_GetKeyDown(Key::Num7)) BuyHome(COL_BLUE);
    }

    void Mix(int a, int b, int r) {
        if (bag[a] <= 0 || bag[b] <= 0) { ShowMix("Need 1 " + ColorName(a) + " and 1 " + ColorName(b) + " to mix " + ColorName(r) + "!"); return; }
        bag[a] -= 1; bag[b] -= 1; bag[r] += 1;
        ShowMix("Concocted " + ColorName(r) + " potion! (" + ColorName(r) + " x" + bag[r] + ")");
    }

    void BuyHome(int c) {
        if (coins < BASE_COST) { ShowMix("Not enough coins for " + ColorName(c) + "."); return; }
        coins -= BASE_COST; bag[c] += 1;
        ShowMix("Bought " + ColorName(c) + " potion. (" + ColorName(c) + " x" + bag[c] + ")");
    }

    // ========================================================= VICTORY =====
    void ShowVictory() {
        TouchContinue();
        string s = "*** GRAND CHAMPION OF POTIONS! ***\n";
        s += "You have traveled through all 10 towns, defeated all rival alchemists,\n";
        s += "and mastered the color wheel alchemy! Total Coins: " + coins + "\n\n";
        s += "Press E or Enter to play again!";
        SetHUD(s);
    }

    void UpdateVictory(float dt) {
        if (Input_GetKeyDown(Key::E) || Input_GetKeyDown(Key::Enter)) {
            townIndex = 0;
            hp = START_HP; maxHP = START_HP;
            mp = START_MP; maxMP = START_MP;
            coins = 12;
            for (uint i = 0; i < beaten.length(); i++) beaten[i] = false;
            bag[0] = 3; bag[1] = 1; bag[2] = 3; bag[3] = 1; bag[4] = 3; bag[5] = 1;
            lastResult = "Starting new Alchemist journey!";
            Begin(MODE_TOWN, -1);
        }
    }
}
