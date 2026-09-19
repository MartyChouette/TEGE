#!/usr/bin/env python3
"""Find a header reachable at two include paths, or a component declared twice.

Two checks, one class of bug: the same thing declared in two places, where the
copies cannot meet and nothing fails to build.

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
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# The first-party PUBLIC include roots, from Core/CMakeLists.txt and
# Engine/CMakeLists.txt. Vendored roots are excluded on purpose: we do not own
# them and cannot resolve a collision inside one.
INCLUDE_ROOTS = ['Core/include', 'Engine/include']

HEADER_SUFFIXES = ('.h', '.hpp', '.hh', '.inl')


# Component structs, wherever they are declared. A component is identified by
# NAME at runtime through the ECS, so two structs sharing a name in different
# namespaces are two different component types that read as one.
#
# SaveLoadMenuComponent was declared in Enjin::ECS (serialized, in the
# inspector, present in every scene) and again in Enjin::Gameplay, and
# DrawSaveLoadMenu took the Gameplay one -- so the draw function could never be
# passed a component any entity actually had. It had no caller anywhere in the
# repo because there could not be one, and the save/load menu was simply never
# drawn in any runtime. Nothing about that fails to compile.
COMPONENT_DECL = re.compile(r'^\s*struct\s+(?:ENJIN_API\s+)?([A-Za-z_]\w*Component)\s*(?:final\s*)?[:{]',
                            re.MULTILINE)

COMPONENT_ROOTS = ['Engine/include', 'Core/include', 'Engine/src', 'Core/src']

# Systems, for the same reason and with a worse failure.
#
# This check exists because the component version of it missed one. It caught
# AudioReactiveComponent declared in both Enjin::ECS and Enjin::Effects, and the
# rename that followed left a SECOND collision standing: there are two classes
# called AudioReactiveSystem, Enjin::Audio (beat clock, RTPC, lip sync, MIDI --
# ticked by PlayMode and both players) and Enjin::Effects (FFT to per-vertex
# mesh displacement -- constructed by nothing, anywhere). The runtimes tick the
# one whose name they recognise, and the other half of the feature has never
# run on any platform.
#
# A system collision is worse than a component one because there is no registry
# to disagree with: a system is wired by someone typing its name, so the wrong
# one is simply the one that got typed, and nothing in the build or the tests
# can tell.
SYSTEM_DECL = re.compile(
    r'^\s*class\s+(?:ENJIN_API\s+)?([A-Za-z_]\w*System)\s*(?:final\s*)?[:{]',
    re.MULTILINE)


def duplicate_declarations(pattern):
    """{name: [paths]} for every name `pattern` matches in more than one file."""
    seen = {}
    for root in COMPONENT_ROOTS:
        base_dir = os.path.join(ROOT, root)
        if not os.path.isdir(base_dir):
            continue
        for base, _dirs, files in os.walk(base_dir):
            for name in files:
                if not name.endswith(('.h', '.hpp', '.cpp')):
                    continue
                path = os.path.join(base, name)
                try:
                    with open(path, encoding='utf-8', errors='replace') as fh:
                        text = fh.read()
                except OSError:
                    continue
                for m in set(pattern.findall(text)):
                    rel = os.path.relpath(path, ROOT).replace(os.sep, '/')
                    seen.setdefault(m, set()).add(rel)
    return {k: sorted(v) for k, v in seen.items() if len(v) > 1}


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
    comps = duplicate_declarations(COMPONENT_DECL)
    systems = duplicate_declarations(SYSTEM_DECL)

    if not dupes and not comps and not systems:
        print('shadowed header audit: none (%d headers across %d roots, '
              'no duplicated component or system declarations)'
              % (len(reachable), len(INCLUDE_ROOTS)))
        return 0

    if systems:
        print('Systems declared in more than one place.')
        print('A system is wired by someone typing its name, so a collision')
        print('means the runtimes tick one of them and the other never runs.')
        for name, paths in sorted(systems.items()):
            print('  class %s' % name)
            for pth in paths:
                print('      %s' % pth)
        print('')

    if comps:
        print('Component structs declared in more than one place.')
        print('Two structs sharing a name are two TYPES that read as one.')
        for name, paths in sorted(comps.items()):
            print('  struct %s' % name)
            for pth in paths:
                print('      %s' % pth)
        print('')

    if (comps or systems) and not dupes:
        return 1 if strict else 0

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
