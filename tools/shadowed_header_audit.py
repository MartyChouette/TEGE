#!/usr/bin/env python3
"""Find a header reachable at the same include path from more than one root.

EnjinCore and EnjinEngine each put their own include/ on the PUBLIC include
path, so both are searched for every `#include "Enjin/..."` in the project and
whichever root is searched first wins. A header present in both is therefore
two different definitions of the same thing, and which one a translation unit
gets depends on search order rather than on anything anybody wrote down.

This is not hypothetical. Enjin/Math/Vector.h existed in both roots for months.
The Engine copy -- the pre-SIMD one -- is what the engine and every test
compiled, so the SIMD math sprint never ran anywhere and the two libraries
disagreed about the inline bodies of a Vector3 they hand each other. Nothing
failed: both copies computed the same answers, every build was green, and the
only thing that surfaced it was a mutation test that broke one copy and watched
all 65 math tests stay green.

A duplicate is worth failing on even when the two files are byte-identical.
Identical today is one edit away from the state above, and that edit looks
correct in review because the reviewer is reading the copy the author edited.

  python tools/shadowed_header_audit.py           # list findings
  python tools/shadowed_header_audit.py --strict  # exit 1 if any exist
"""
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# The first-party PUBLIC include roots, from Core/CMakeLists.txt and
# Engine/CMakeLists.txt. Vendored roots are excluded on purpose: we do not own
# them and cannot resolve a collision inside one.
INCLUDE_ROOTS = ['Core/include', 'Engine/include']

HEADER_SUFFIXES = ('.h', '.hpp', '.hh', '.inl')


def main():
    strict = '--strict' in sys.argv
    reachable = {}
    for root in INCLUDE_ROOTS:
        base_dir = os.path.join(ROOT, root)
        if not os.path.isdir(base_dir):
            print('missing include root: %s' % root)
            return 1
        for base, _dirs, files in os.walk(base_dir):
            for name in files:
                if not name.endswith(HEADER_SUFFIXES):
                    continue
                path = os.path.join(base, name)
                include_path = os.path.relpath(path, base_dir).replace(os.sep, '/')
                reachable.setdefault(include_path, []).append(
                    os.path.relpath(path, ROOT).replace(os.sep, '/'))

    dupes = {k: v for k, v in reachable.items() if len(v) > 1}
    if not dupes:
        print('shadowed header audit: none (%d headers across %d roots)'
              % (len(reachable), len(INCLUDE_ROOTS)))
        return 0

    print('Headers reachable at the SAME include path from more than one root.')
    print('Which one a translation unit gets depends on include search order.\n')
    for include_path, paths in sorted(dupes.items()):
        print('  #include "%s"' % include_path)
        for p in paths:
            print('      %s  (%d bytes)' % (p, os.path.getsize(os.path.join(ROOT, p))))
    print('\n%d collision(s). Keep ONE, merge anything unique to the other into it,'
          '\nand delete the loser -- identical copies count, they are one edit from'
          '\ndiverging and the divergence is invisible.' % len(dupes))
    return 1 if strict else 0


if __name__ == '__main__':
    sys.exit(main())
