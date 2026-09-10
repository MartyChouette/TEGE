// PotionData.as — the color/potion model shared by town and battle.
// Six colors on a wheel; opposites are the hard counters (soft type chart:
// 2x / 1x / 0.5x). Strength and MP come from the original game's table.
// Include this from any script that needs the color rules:  #include "PotionData.as"

// Wheel order chosen so the opposite of a color is (i + 3) % 6:
//   RED<->GREEN, ORANGE<->BLUE, YELLOW<->PURPLE
const int COL_RED    = 0;
const int COL_ORANGE = 1;
const int COL_YELLOW = 2;
const int COL_GREEN  = 3;
const int COL_BLUE   = 4;
const int COL_PURPLE = 5;
const int COL_COUNT  = 6;

string ColorName(int c) {
    switch (c) {
        case COL_RED:    return "Red";
        case COL_ORANGE: return "Orange";
        case COL_YELLOW: return "Yellow";
        case COL_GREEN:  return "Green";
        case COL_BLUE:   return "Blue";
        case COL_PURPLE: return "Purple";
    }
    return "?";
}

// Lowercase slug used in asset filenames, e.g. "assets/potion_" + ColorSlug(c) + ".png".
string ColorSlug(int c) {
    switch (c) {
        case COL_RED:    return "red";
        case COL_ORANGE: return "orange";
        case COL_YELLOW: return "yellow";
        case COL_GREEN:  return "green";
        case COL_BLUE:   return "blue";
        case COL_PURPLE: return "purple";
    }
    return "red";
}

// RGB for tinting placeholder sprites and, later, the potion liquid.
Vector3 ColorRGB(int c) {
    switch (c) {
        case COL_RED:    return Vector3(0.90f, 0.20f, 0.20f);
        case COL_ORANGE: return Vector3(0.95f, 0.55f, 0.15f);
        case COL_YELLOW: return Vector3(0.95f, 0.85f, 0.20f);
        case COL_GREEN:  return Vector3(0.25f, 0.75f, 0.30f);
        case COL_BLUE:   return Vector3(0.20f, 0.45f, 0.90f);
        case COL_PURPLE: return Vector3(0.55f, 0.25f, 0.75f);
    }
    return Vector3(1.0f, 1.0f, 1.0f);
}

// Base throw strength (from the original table).
int ColorStrength(int c) {
    switch (c) {
        case COL_RED:    return 3;
        case COL_ORANGE: return 6;
        case COL_YELLOW: return 2;
        case COL_GREEN:  return 4;
        case COL_BLUE:   return 2;
        case COL_PURPLE: return 5;
    }
    return 1;
}

// MP cost to throw a color. Red costs 3, everything else 2 (original table).
int ColorMPCost(int c) {
    return (c == COL_RED) ? 3 : 2;
}

// True for the three colors you buy; the rest you mix.
bool IsPrimary(int c) {
    return c == COL_RED || c == COL_YELLOW || c == COL_BLUE;
}

// Mix two distinct primaries into the secondary between them (subtractive paint).
// Returns -1 if the pair isn't two distinct primaries.
int MixPrimaries(int a, int b) {
    if (a == b) return -1;
    bool red    = (a == COL_RED)    || (b == COL_RED);
    bool yellow = (a == COL_YELLOW) || (b == COL_YELLOW);
    bool blue   = (a == COL_BLUE)   || (b == COL_BLUE);
    if (red && yellow) return COL_ORANGE;
    if (yellow && blue) return COL_GREEN;
    if (blue && red)   return COL_PURPLE;
    return -1;
}

// The soft type chart. Opposite = 2x, neighbor = 0.5x, otherwise 1x.
float TypeMultiplier(int attacker, int defender) {
    int delta = (((defender - attacker) % COL_COUNT) + COL_COUNT) % COL_COUNT;
    if (delta == 3) return 2.0f;                 // opposite — the hard counter
    if (delta == 1 || delta == 5) return 0.5f;   // neighbor — too similar to hurt
    return 1.0f;                                 // delta 0, 2, or 4
}

// Damage one thrown color deals to the other this exchange.
int PotionDamage(int myColor, int theirColor) {
    float raw = float(ColorStrength(myColor)) * TypeMultiplier(myColor, theirColor);
    return int(raw + 0.5f);
}

// Food & Healing Item definitions
const int ITEM_BERRIES  = 0;
const int ITEM_SALAD    = 1;
const int ITEM_BAGUETTE = 2;
const int ITEM_COUNT    = 3;

string ItemName(int it) {
    switch (it) {
        case ITEM_BERRIES:  return "Berries (+5 HP)";
        case ITEM_SALAD:    return "Salad (+10 HP)";
        case ITEM_BAGUETTE: return "Baguette (Full HP)";
    }
    return "?";
}

int ItemHeal(int it) {
    switch (it) {
        case ITEM_BERRIES:  return 5;
        case ITEM_SALAD:    return 10;
        case ITEM_BAGUETTE: return 999;
    }
    return 0;
}

int ItemCost(int it) {
    switch (it) {
        case ITEM_BERRIES:  return 4;
        case ITEM_SALAD:    return 7;
        case ITEM_BAGUETTE: return 12;
    }
    return 0;
}

