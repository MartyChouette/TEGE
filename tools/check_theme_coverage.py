#!/usr/bin/env python3
"""Report how much of the editor's colour actually goes through EditorTheme.h.

EditorTheme.h used to open with "Change these to re-theme the entire editor".
That was false: the editor draws with 548 IM_COL32 call sites and 35 of them
went through the theme, so changing a constant moved about six per cent of it.
The claim was not a lie anybody told on purpose -- it was true of the intention
and never measured against the code.

So it is measured now. Run this and the header's numbers are checkable rather
than asserted.

    python tools/check_theme_coverage.py            report
    python tools/check_theme_coverage.py --max-new N  fail if more than N
                                                      unnamed colours appear in
                                                      MORE THAN ONE file

The gate is deliberately on cross-file colours only. A colour used once, in one
place, is a local decision and naming it would be noise; a colour that shows up
in two files is a shared one whose two copies will drift.
"""
import argparse
import collections
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
THEME_H = os.path.join(ROOT, "Engine", "include", "Enjin", "Editor", "EditorTheme.h")
SRC_DIR = os.path.join(ROOT, "Engine", "src", "Editor")

CONST_RE = re.compile(
    r"constexpr ImU32\s+(\w+)\s*=\s*IM_COL32\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\)")
LIT_RE = re.compile(r"IM_COL32\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\)")
USE_RE = re.compile(r"\bTheme::(\w+)\b")


def theme_values():
    out = {}
    with open(THEME_H, encoding="utf-8") as f:
        for line in f:
            m = CONST_RE.search(line)
            if m:
                out[tuple(int(m.group(i)) for i in range(2, 6))] = m.group(1)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--max-new", type=int, default=None,
                    help="fail if more than N unnamed colours are used in 2+ files")
    args = ap.parse_args()

    named = theme_values()

    theme_uses = 0
    literal_sites = 0
    exact_dupes = 0
    unnamed = collections.Counter()
    unnamed_files = collections.defaultdict(set)

    for root, _, files in os.walk(SRC_DIR):
        for fn in sorted(files):
            if not fn.endswith(".cpp"):
                continue
            path = os.path.join(root, fn)
            with open(path, encoding="utf-8", errors="replace") as f:
                text = f.read()
            theme_uses += len(USE_RE.findall(text))
            for m in LIT_RE.finditer(text):
                literal_sites += 1
                v = tuple(int(m.group(i)) for i in range(1, 5))
                if v in named:
                    exact_dupes += 1
                else:
                    unnamed[v] += 1
                    unnamed_files[v].add(fn)

    # A literal that spells out a colour the theme already names is a site that
    # should be using the constant and is not. That is the drift this catches,
    # and it is counted in the same walk above rather than a second one.

    cross_file = {v: n for v, n in unnamed.items() if len(unnamed_files[v]) >= 2}
    once_only = {v: n for v, n in unnamed.items() if n == 1}

    total = theme_uses + literal_sites
    print("Editor colour sites        : %d" % total)
    print("  through Theme::          : %d  (%.0f%%)" %
          (theme_uses, 100.0 * theme_uses / total if total else 0))
    print("  raw IM_COL32 literals    : %d" % literal_sites)
    print()
    print("Named constants in theme   : %d" % len(named))
    print("Unnamed distinct colours   : %d" % len(unnamed))
    print("  used in 2+ files         : %d  across %d sites" %
          (len(cross_file), sum(cross_file.values())))
    print("  used exactly once        : %d" % len(once_only))
    print()
    if exact_dupes:
        print("WARNING: %d literal(s) spell out a colour the theme already names." % exact_dupes)
        print("         Those should use the constant; that is the drift this catches.")

    if cross_file:
        print("Unnamed colours shared across files (candidates for a constant):")
        for v, n in sorted(cross_file.items(), key=lambda kv: -kv[1])[:20]:
            print("   %-24s x%-3d %s" % (str(v), n, sorted(unnamed_files[v])))

    if args.max_new is not None and len(cross_file) > args.max_new:
        print()
        print("FAIL: %d unnamed cross-file colours, limit is %d" %
              (len(cross_file), args.max_new))
        return 1
    if exact_dupes:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
