#!/usr/bin/env python3
"""Generate the material flag-word defines into the shaders from one table.

adr-0008 phase 3. triangle.vert and triangle.frag read the SAME push-constant
`flags` word and each used to carry a hand-written `#define FLAG_*` list. Two
bits drifted into meaning different things per stage (bit 3 SKINNED/SDF_TEXT,
bit 4 WIND_SWAY/EXCLUDE_CEL), which is invisible to every other check this
project has: a wrong bit compiles, links, passes the suite, and renders a
picture that is merely wrong.

The table lives in Engine/include/Enjin/Renderer/MaterialFlagBits.h. This tool
rewrites the marked block in each shader from it.

    python tools/gen_material_flags.py            # rewrite the shaders
    python tools/gen_material_flags.py --check    # CI: fail if drifted

Why a rewritten block rather than a shared #include: triangle.* are compiled
with glslangValidator, which rejects #include (that is why the RT shaders need
glslc). Inlining a generated block keeps one source of truth without changing
how these shaders build.
"""

import argparse
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HEADER = os.path.join(ROOT, 'Engine', 'include', 'Enjin', 'Renderer', 'MaterialFlagBits.h')
SHADERS = {
    'vert': os.path.join(ROOT, 'Engine', 'shaders', 'triangle.vert'),
    'frag': os.path.join(ROOT, 'Engine', 'shaders', 'triangle.frag'),
}

BEGIN = '// BEGIN GENERATED MATERIAL FLAGS -- edit MaterialFlagBits.h, not this'
END = '// END GENERATED MATERIAL FLAGS'

ROW = re.compile(r'X\(\s*([A-Z0-9_]+)\s*,\s*(\d+)\s*,\s*([01])\s*,\s*([01])\s*\)')


def read_table():
    """Parse the X-macro rows. Returns [(name, bit, vert, frag)]."""
    with open(HEADER, encoding='utf-8') as f:
        text = f.read()
    start = text.index('#define ENJIN_MATERIAL_FLAG_BITS(X)')
    end = text.index('// clang-format on', start)
    rows = [(m.group(1), int(m.group(2)), m.group(3) == '1', m.group(4) == '1')
            for m in ROW.finditer(text[start:end])]
    if not rows:
        sys.exit('gen_material_flags: no rows parsed from MaterialFlagBits.h')

    # A bit claimed twice is the defect this whole file exists to prevent, so
    # refuse to generate rather than emit a shader that compiles and lies.
    seen = {}
    for name, bit, _, _ in rows:
        if bit in seen:
            sys.exit('gen_material_flags: bit %d claimed by both %s and %s'
                     % (bit, seen[bit], name))
        seen[bit] = name
        if not 0 <= bit <= 31:
            sys.exit('gen_material_flags: %s has bit %d, outside 0..31' % (name, bit))
    return rows


def block_for(stage, rows):
    used = [r for r in rows if (r[2] if stage == 'vert' else r[3])]
    width = max(len(n) for n, _, _, _ in used)
    lines = [BEGIN,
             '// Generated from Engine/include/Enjin/Renderer/MaterialFlagBits.h.',
             '// Both stages read ONE push-constant word; a bit means the same thing',
             '// in each, and this block is what keeps that true.']
    for name, bit, _, _ in sorted(used, key=lambda r: r[1]):
        lines.append('#define FLAG_%-*s (1 << %d)' % (width, name, bit))
    lines.append(END)
    return '\n'.join(lines)


def apply(stage, path, rows, check):
    with open(path, encoding='utf-8') as f:
        text = f.read()
    want = block_for(stage, rows)

    if BEGIN in text:
        start = text.index(BEGIN)
        stop = text.index(END, start) + len(END)
        have = text[start:stop]
        if have == want:
            return False
        if check:
            print('DRIFT: %s does not match MaterialFlagBits.h' % os.path.basename(path))
            return True
        text = text[:start] + want + text[stop:]
    else:
        # First run: replace the hand-written defines with the generated block,
        # inserted where the first of them was so ordering is preserved.
        hand = [m for m in re.finditer(r'^#define FLAG_[A-Z0-9_]+\s+\(1 << \d+\)\s*$',
                                       text, re.M)]
        if not hand:
            sys.exit('gen_material_flags: %s has neither markers nor FLAG_ defines'
                     % os.path.basename(path))
        if check:
            print('DRIFT: %s has no generated block' % os.path.basename(path))
            return True
        at = hand[0].start()
        # Drop every hand-written define, last first so offsets stay valid.
        for m in reversed(hand):
            text = text[:m.start()] + text[m.end():]
            if text[m.start():m.start() + 1] == '\n':
                text = text[:m.start()] + text[m.start() + 1:]
        text = text[:at] + want + '\n' + text[at:]

    if not check:
        with open(path, 'w', encoding='utf-8', newline='\n') as f:
            f.write(text)
        print('wrote %s' % os.path.basename(path))
    return True


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--check', action='store_true',
                    help='report drift and exit nonzero instead of rewriting')
    args = ap.parse_args()

    rows = read_table()
    drifted = [apply(stage, path, rows, args.check) for stage, path in SHADERS.items()]

    if args.check:
        if any(drifted):
            print('\nRun: python tools/gen_material_flags.py')
            return 1
        print('material flag defines match MaterialFlagBits.h (%d bits)' % len(rows))
        return 0
    if not any(drifted):
        print('already up to date (%d bits)' % len(rows))
    return 0


if __name__ == '__main__':
    sys.exit(main())
