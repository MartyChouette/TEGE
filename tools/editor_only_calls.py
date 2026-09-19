#!/usr/bin/env python3
"""Find an engine capability whose only caller is the EDITOR.

Three separate features were found this way on 2026-09-19, all with the same
shape and all invisible to every other check:

  - `PostProcessing::ApplyTAA` had exactly one call site, in EditorLayer.cpp.
    Temporal antialiasing therefore existed in the editor viewport and in no
    shipped game, while the AA menu offered it everywhere.
  - `IUpscaler::Dispatch` likewise. Selecting FSR2 in a build injected jitter
    and never upscaled, which is worse than leaving it off.
  - The player applied its scene's post-process settings with a null
    PostProcessing pointer, so colour grading, vignette, bloom, tone mapping
    and the AA mode reached no exported game at all.

Every one of those compiled, passed every test, and previewed CORRECTLY while
authoring. That is the cruel direction: the one place a person checks is the
one place it works, so the bug ships and nobody sees it until a player does.

This is tools/runtime_parity.py applied to CALLS rather than to system ticks.
runtime_parity asks "is this system updated in all three runtimes"; this asks
"is this capability ever reached outside the editor at all".

A hit is not automatically a bug. Plenty of engine API is legitimately
editor-only: gizmo maths, asset import, the build pipeline, anything an
exported game has no business calling. So this keeps a BASELINE of known and
accepted cases, the way backend_parity and runtime_parity do, and fails only
when the set GROWS.

    python tools/editor_only_calls.py              # report
    python tools/editor_only_calls.py --strict     # non-zero if it grew
    python tools/editor_only_calls.py --update     # accept the current set
"""
import argparse
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BASELINE = os.path.join(ROOT, 'tools', 'editor_only_calls_baseline.json')

# Headers whose API an exported game should be able to reach. Enjin/Editor is
# deliberately absent: that IS the editor's own API and being editor-only is
# what it is for.
HEADER_DIRS = [
    'Engine/include/Enjin/Renderer',
    'Engine/include/Enjin/ECS',
    'Engine/include/Enjin/Effects',
    'Engine/include/Enjin/Gameplay',
    'Engine/include/Enjin/Animation',
    'Engine/include/Enjin/Audio',
    'Engine/include/Enjin/GUI',
    'Engine/include/Enjin/Physics',
]

# Headers that are not C++ API at all. WebShaderData.h is WGSL source held in
# string literals, so the declaration regex happily reported shader functions
# like `fract` as engine methods that only the editor calls.
SKIP_HEADERS = ('WebShaderData.h', 'ShaderData.h', 'RTShaderData.h')

EDITOR_SRC = ('Engine/src/Editor',)
# Where a shipped game's code lives. A call from any of these clears a method.
RUNTIME_SRC = (
    'Player/src',
    'Engine/src/ECS',
    'Engine/src/Renderer',
    'Engine/src/Effects',
    'Engine/src/Gameplay',
    'Engine/src/Animation',
    'Engine/src/Audio',
    'Engine/src/GUI',
    'Engine/src/Physics',
    'Engine/src/Scene',
    'Engine/src/Scripting',
    'Engine/src/Networking',
    'Engine/src/Build',
    'Engine/src/AI',
    'Engine/src/Assets',
    'Core/src',
)

# A method declaration: `ReturnType Name(...)` inside a class body. Const,
# override and default-argument noise is ignored; only the NAME is wanted.
DECL = re.compile(
    r'^\s{2,}(?:virtual\s+|static\s+|inline\s+|explicit\s+|ENJIN_API\s+)*'
    r'(?:const\s+)?[A-Za-z_][\w:<>,\s\*&]*?[\s\*&]'
    r'(?P<name>[A-Za-z_]\w*)\s*\([^;]*\)\s*(?:const\s*)?(?:override\s*)?(?:noexcept\s*)?[;{]',
    re.MULTILINE)

# Call sites. Two forms, and missing the second one is what made the first
# version of this tool wrong:
#
#   obj->Name(  /  obj.Name(     a call through an instance
#   Name(                        a call from INSIDE the same class, or a free
#                                function -- no prefix at all
#
# TieredSaveSystem::SyncToCloud is reached from TieredSaveSystem's own Update,
# written plainly as `SyncToCloud();`, so the prefix-only version reported it as
# editor-only when a runtime reaches it every frame. A guard that cries wolf
# gets its baseline padded with things that are fine, which is how it stops
# catching the things that are not.
#
# `(?<![:\w])` keeps the method's own DEFINITION out: `void Type::Name(` has a
# colon immediately before the name, and `OtherName(` has a word character.
# Collected ONCE per corpus rather than searched once per name. A regex per
# name across two multi-megabyte blobs is O(names x text) and took minutes;
# this is one pass and takes seconds.
CALL_PREFIXED = re.compile(r'(?:->|\.)([A-Za-z_]\w*)\s*\(')
CALL_BARE = re.compile(r'(?<![:\w.>])([A-Za-z_]\w*)\s*\(')


def called_names(text):
    """Every function name that `text` appears to CALL."""
    names = set(CALL_PREFIXED.findall(text))
    names.update(CALL_BARE.findall(text))
    return names

# Names too generic to attribute to one class, or inherited from the standard
# library, where a text search cannot tell one owner from another.
SKIP_NAMES = {
    'Get', 'Set', 'Update', 'Render', 'Init', 'Initialize', 'Shutdown', 'Clear',
    'Reset', 'Begin', 'End', 'Apply', 'Draw', 'Add', 'Remove', 'Create',
    'Destroy', 'Load', 'Save', 'Start', 'Stop', 'Run', 'Tick', 'Size', 'Empty',
    'Data', 'At', 'Find', 'Count', 'Resize', 'Name', 'Value', 'Type', 'Bind',
}


def iter_files(dirs, exts=('.cpp', '.h', '.hpp')):
    for d in dirs:
        base = os.path.join(ROOT, d)
        if not os.path.isdir(base):
            continue
        for root, _dirs, files in os.walk(base):
            if '.claude' in root or 'worktrees' in root:
                continue
            for f in files:
                if f.endswith(exts):
                    yield os.path.join(root, f)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--strict', action='store_true')
    ap.add_argument('--update', action='store_true')
    args = ap.parse_args()

    # Declared names worth checking.
    names = {}
    for path in iter_files(HEADER_DIRS, ('.h', '.hpp')):
        try:
            text = open(path, encoding='utf-8', errors='replace').read()
        except OSError:
            continue
        rel = os.path.relpath(path, ROOT).replace(os.sep, '/')
        if os.path.basename(path) in SKIP_HEADERS:
            continue
        for m in DECL.finditer(text):
            n = m.group('name')
            if n in SKIP_NAMES or len(n) < 5:
                continue
            names.setdefault(n, rel)

    editor_text = []
    for path in iter_files(EDITOR_SRC, ('.cpp',)):
        try:
            editor_text.append(open(path, encoding='utf-8', errors='replace').read())
        except OSError:
            pass
    editor_blob = '\n'.join(editor_text)

    runtime_text = []
    for path in iter_files(RUNTIME_SRC, ('.cpp',)):
        try:
            runtime_text.append(open(path, encoding='utf-8', errors='replace').read())
        except OSError:
            pass
    runtime_blob = '\n'.join(runtime_text)

    editor_calls = called_names(editor_blob)
    runtime_calls = called_names(runtime_blob)

    hits = {}
    for n, decl_path in sorted(names.items()):
        if n not in editor_calls:
            continue                     # never called by the editor either
        if n in runtime_calls:
            continue                     # a runtime reaches it: fine
        hits[n] = decl_path

    if args.update:
        with open(BASELINE, 'w', encoding='utf-8', newline='') as f:
            json.dump({'editor_only': hits}, f, indent=2, sort_keys=True)
        print('baseline updated: %d editor-only call sites' % len(hits))
        return 0

    base = {}
    if os.path.exists(BASELINE):
        with open(BASELINE, encoding='utf-8') as f:
            base = json.load(f).get('editor_only', {})

    added = {k: v for k, v in hits.items() if k not in base}
    gone = [k for k in base if k not in hits]

    print('editor-only calls: %d (baseline %d)' % (len(hits), len(base)))
    if gone:
        print('\nNo longer editor-only (a runtime now reaches these):')
        for k in sorted(gone):
            print('  %s' % k)
    if added:
        print('\nNEW editor-only capabilities. Each is reachable while authoring')
        print('and unreachable in a shipped game. Wire it into the runtimes, or')
        print('accept it with --update if it is genuinely editor-side:')
        for k in sorted(added):
            print('  %-40s %s' % (k, added[k]))
        return 1 if args.strict else 0

    if not base:
        print('\nNo baseline yet. Review the list and run --update to accept it.')
        for k in sorted(hits):
            print('  %-40s %s' % (k, hits[k]))
    return 0


if __name__ == '__main__':
    sys.exit(main())
