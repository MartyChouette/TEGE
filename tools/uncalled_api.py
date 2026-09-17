#!/usr/bin/env python3
"""Public engine methods that nothing ever calls.

Four dead features were found by hand in one subsystem on 2026-09-16, and every
one had the same shape: a method that exists, compiles, is documented, and has
no callers. `ConfigureAutoSave` was the switch that turned auto-save on, and
nothing called it, so auto-save had never run. `OnSceneTransition` populated a
whole tier of the save system, and nothing called it, so that tier was never
populated. `OnCheckpointReached` is still like that.

Nothing reports this. A method with no callers compiles clean, passes every
test, and is invisible to the capture harness because the feature behind it
renders nothing precisely by not running.

    python tools/uncalled_api.py Gameplay        # one subsystem
    python tools/uncalled_api.py                 # all of Engine/include

HOW IT DECIDES. A method is "uncalled" when the only places its name appears
followed by `(` are its own declaration and its own definition. That is the test
done by hand for the four above.

WHAT IT CANNOT SEE, and why the output is a LEAD rather than a verdict:

  - virtual methods reached through a base pointer, so anything overriding or
    overridden is skipped rather than reported;
  - methods called from AngelScript or the visual-script registry, where the
    call site is a registration string rather than a call;
  - methods taken as a function pointer or bound into a std::function;
  - constructors, destructors and operators, which are skipped.

All of those make it UNDER-report, which is the right direction: everything it
names really has no textual caller, and the true figure is at least this. It has
still been wrong -- read a name it prints and check it before believing it. The
sibling tool tools/field_coverage.py was confidently wrong five times in one
sitting for exactly this kind of reason.
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HEADERS = os.path.join(ROOT, 'Engine', 'include', 'Enjin')

# Everywhere a call could come from. Tests count: a method only a test calls is
# not dead, it is untested-in-production, which is a different conversation.
SOURCE_DIRS = [
    os.path.join(ROOT, 'Engine', 'src'),
    os.path.join(ROOT, 'Engine', 'include'),
    os.path.join(ROOT, 'Editor', 'src'),
    os.path.join(ROOT, 'Player', 'src'),
    os.path.join(ROOT, 'Core', 'src'),
    os.path.join(ROOT, 'Core', 'include'),
    os.path.join(ROOT, 'Tests'),
    os.path.join(ROOT, 'Examples'),
]

# A method declaration inside a class body: returns something, takes something.
# Deliberately narrow -- it would rather miss a declaration than invent one.
DECL_RE = re.compile(
    r'^\s{2,}'                                  # indented, so inside a class
    r'(?!return\b|if\b|for\b|while\b|else\b|case\b|delete\b)'
    r'(?:static\s+|inline\s+|constexpr\s+|virtual\s+|explicit\s+)*'
    r'(?:const\s+)?'
    r'[A-Za-z_][\w:<>,\s\*&]*?'                 # return type
    r'[\s\*&]'
    r'([A-Za-z_]\w*)\s*\('                      # the name
    , re.M)

SKIP_NAMES = {'operator', 'if', 'for', 'while', 'switch', 'return', 'sizeof', 'catch'}


def strip_comments(src):
    """C++ comments out, string literals intact. Single pass.

    The two-regex version of this (block comments with DOTALL, then line
    comments) ate live code in a sibling tool: a `/*` inside a `//` comment
    opened a block that closed hundreds of lines later.
    """
    out = []
    i, n = 0, len(src)
    while i < n:
        c = src[i]
        if c in '"\'':
            quote = c
            out.append(c); i += 1
            while i < n:
                out.append(src[i])
                if src[i] == '\\':
                    if i + 1 < n: out.append(src[i + 1])
                    i += 2; continue
                if src[i] == quote:
                    i += 1; break
                i += 1
            continue
        if c == '/' and i + 1 < n:
            if src[i + 1] == '/':
                while i < n and src[i] != '\n': i += 1
                continue
            if src[i + 1] == '*':
                i += 2
                while i + 1 < n and not (src[i] == '*' and src[i + 1] == '/'): i += 1
                i += 2
                out.append(' ')
                continue
        out.append(c); i += 1
    return ''.join(out)


def sources():
    """Every source file, stripped, keyed by path."""
    out = {}
    for d in SOURCE_DIRS:
        for base, _, files in os.walk(d):
            for f in files:
                if not f.endswith(('.cpp', '.h', '.hpp', '.inl')):
                    continue
                p = os.path.join(base, f)
                try:
                    with open(p, encoding='utf-8', errors='replace') as fh:
                        out[p] = strip_comments(fh.read())
                except OSError:
                    pass
    return out


def main():
    subsystem = sys.argv[1] if len(sys.argv) > 1 else ''
    root = os.path.join(HEADERS, subsystem) if subsystem else HEADERS
    if not os.path.isdir(root):
        print('no such subsystem: %s' % root)
        return 2

    all_src = sources()

    # Names that appear anywhere as `virtual`, an override, or a script
    # registration are excluded wholesale rather than judged: a virtual is
    # reached through a base pointer and a bound name through a string.
    virtualish = set()
    for text in all_src.values():
        for m in re.finditer(r'\bvirtual\b[^;{]*?([A-Za-z_]\w*)\s*\(', text):
            virtualish.add(m.group(1))
        for m in re.finditer(r'\boverride\b', text):
            pass
        for m in re.finditer(r'(?:RegisterGlobalFunction|RegisterObjectMethod)\s*\(\s*"[^"]*?([A-Za-z_]\w*)\s*\(', text):
            virtualish.add(m.group(1))

    declared = {}
    for p, text in all_src.items():
        if not p.startswith(root):
            continue
        for m in DECL_RE.finditer(text):
            name = m.group(1)
            if name in SKIP_NAMES or name in virtualish:
                continue
            if name.startswith('~') or name[0].isupper() is False and name.islower() and len(name) < 3:
                continue
            declared.setdefault(name, p)

    uncalled = []
    for name, header in sorted(declared.items()):
        call_re = re.compile(r'\b' + re.escape(name) + r'\s*\(')
        sites = 0
        for p, text in all_src.items():
            hits = len(call_re.findall(text))
            if not hits:
                continue
            # Its own declaration, and its own definition, are not calls.
            if p == header:
                hits -= len(DECL_RE.findall(text)) and \
                        len([1 for m in DECL_RE.finditer(text) if m.group(1) == name])
            hits -= len(re.findall(r'\b\w+::' + re.escape(name) + r'\s*\(', text))
            sites += max(0, hits)
        if sites == 0:
            uncalled.append((name, os.path.relpath(header, ROOT)))

    print('%d method(s) declared under %s with no textual caller:\n'
          % (len(uncalled), os.path.relpath(root, ROOT)))
    for name, header in uncalled:
        print('  %-40s %s' % (name, header))
    print()
    print('READ THESE BEFORE BELIEVING THEM. This is a lead, not a verdict: it cannot')
    print('see virtual dispatch, script-bound names, or function pointers, and those')
    print('are skipped rather than reported. Check a name by hand the way the four')
    print('real ones were found -- grep it, and see whether anything invokes it.')
    print()
    print('AND GREP CAREFULLY. The first hand-check of this list used `grep "Name("`')
    print('and found callers for five methods that have none: Difficulty_RecordDeath')
    print('CONTAINS RecordDeath as a substring and is an entirely different function.')
    print('A word boundary turns 2 callers into 0. The sloppy check nearly buried a')
    print('real result.')
    print()
    print('THREE THINGS THIS FINDS, and only the first is a dead feature:')
    print('  1. A switch nothing flips. ConfigureAutoSave was the enable for auto-save')
    print('     and had no callers, so auto-save had never run.')
    print('  2. Redundant API. DynamicDifficultySystem::RecordDeath and friends have no')
    print('     callers, but dynamic difficulty WORKS -- the script bindings increment')
    print('     the component directly and the system reads it. Dead surface, live')
    print('     feature, and a second way to do one thing.')
    print('  3. A reporter nobody reads. SavePointSystem::GetMessage exists so a HUD')
    print('     can show a save confirmation, and no HUD does.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
