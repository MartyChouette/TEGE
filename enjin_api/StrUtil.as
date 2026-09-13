// StrUtil.as - the string helpers every data-driven script ends up rewriting.
//
// WHY THIS EXISTS. TEGE calls RegisterStdString but not RegisterStdStringUtils,
// and string::split() lives in the utils half, so THERE IS NO SPLIT ANYWHERE in
// AngelScript here. Every script that reads a joined string has grown its own
// private one, commonly as a class method. These are the same helpers as
// globals, once, so shared code can use them.
//
// parseInt / parseUInt / parseFloat / formatInt / formatFloat DO exist - they are
// registered in RegisterStdString_Native, which RegisterStdString calls. ToInt and
// ToFloat below are kept anyway because they never throw and they skip stray
// separators, which is what reading hand-authored joined data actually needs.
//
// The joined-string convention they serve: records separated by "|", fields
// within a record by ";". It exists because the Player's .enjdata loader only
// understands string / float / int / bool and silently DROPS StringArray and
// FloatArray, so a list that must survive a build has to travel as a string.

// Split "a;b;c" into its parts. An empty input gives one empty part, which is
// what you want: a slot with no options is one blank option, not zero.
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

int StrDigit(const string &in c) {
    string digits = "0123456789";
    for (uint i = 0; i < 10; i++) if (digits.substr(i, 1) == c) return int(i);
    return -1;
}

// Non-digits are skipped rather than rejected, so "72" and " 72 " and "72;" all
// read as 72. An empty or digitless string is 0.
int ToInt(const string &in s) {
    int v = 0; bool neg = false, any = false;
    for (uint i = 0; i < s.length(); i++) {
        string c = s.substr(i, 1);
        if (i == 0 && c == "-") { neg = true; continue; }
        int d = StrDigit(c);
        if (d < 0) continue;
        v = v * 10 + d; any = true;
    }
    if (!any) return 0;
    return neg ? -v : v;
}

// One decimal point, everything after it fractional. "-1.5" and "0" and "" all
// behave. Deltas in a reaction table are small and hand-authored, so this is
// enough and it cannot throw.
float ToFloat(const string &in s) {
    float v = 0.0f, scale = 0.0f;
    bool neg = false, any = false;
    for (uint i = 0; i < s.length(); i++) {
        string c = s.substr(i, 1);
        if (i == 0 && c == "-") { neg = true; continue; }
        if (c == "." && scale == 0.0f) { scale = 1.0f; continue; }
        int d = StrDigit(c);
        if (d < 0) continue;
        any = true;
        if (scale == 0.0f) {
            v = v * 10.0f + float(d);
        } else {
            scale = scale * 0.1f;
            v = v + float(d) * scale;
        }
    }
    if (!any) return 0.0f;
    return neg ? -v : v;
}

// Trim ASCII whitespace off both ends. Authored data picked up by hand in an
// inspector field collects trailing spaces and they break every == comparison.
string Trim(const string &in s) {
    uint a = 0, b = s.length();
    while (a < b) {
        string c = s.substr(a, 1);
        if (c != " " && c != "\t" && c != "\n" && c != "\r") break;
        a++;
    }
    while (b > a) {
        string c = s.substr(b - 1, 1);
        if (c != " " && c != "\t" && c != "\n" && c != "\r") break;
        b--;
    }
    return s.substr(a, b - a);
}

string Join(const array<string> &in parts, const string &in sep) {
    string s = "";
    for (uint i = 0; i < parts.length(); i++)
        s += (i > 0 ? sep : "") + parts[i];
    return s;
}
