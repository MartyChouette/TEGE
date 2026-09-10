#!/usr/bin/env python3
"""Every script function named in a doc code sample must actually exist.

A beginner copies a sample out of a tutorial. If the name is wrong the script
does not compile, and the error points at THEIR file, not at ours -- so the
failure reads as "I did it wrong" rather than "the doc is wrong". That is the
worst possible place to put a mistake.

This found eleven of them on its first run (2026-09-10), all in TUTORIALS.md:
Input_IsKeyPressed and Input_IsKeyDown (the real names are Input_GetKeyDown and
Input_GetKey), Event_Fire/Event_Subscribe (Events_Send/Events_Listen),
Weather_SetType/SetWindDirection/SetWindStrength/TriggerLightning,
Quest_Complete, Subtitle_ShowWithSpeaker and Animator_SetBlendWeight. The sprite
tutorial's whole sample used Sprite2D_* and AnimSprite_*, neither of which is a
prefix the engine registers.

    python tools/check_doc_api.py          # exits 1 if a doc names something unreal

Compares against what Engine/src/Scripting/ actually registers plus whatever the
embedded enjin_api/*.as scripts define, so an api-side helper counts as real.
"""
import collections
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

DOCS = [
    'docs/SCRIPTING_API.md',
    'docs/TUTORIALS.md',
    'docs/USER_MANUAL.md',
    'docs/BEGINNERS_GUIDE.md',
    'docs/API_REFERENCE.md',
    'docs/FAQ.md',
]

# Only names that look like engine script bindings. A doc is free to write
# my_helper() in a sample without this complaining about it.
PREFIXES = re.compile(
    r'^(Audio|AudioGraph|Sprite|SpriteAnim|Animator|Input|InputAction|Physics|'
    r'Scene|Entity|Transform|Debug|Time|Material|Light|Camera|Health|Controller|'
    r'Render|Weather|Water|Particle|Save|Quest|Dialogue|HUD|Text|Tween|Events|'
    r'Noise|UI|DataAsset|VisualScript|Touch|Subtitle|Coroutine|Viewmodel|'
    r'Ragdoll|PPVolume)_')


def registered_names():
    names = set()
    out = subprocess.run(
        ['git', 'grep', '-h', '-oE',
         r'"[A-Za-z_:<>0-9 @&]+ [A-Za-z_]+_[A-Za-z_0-9]+\(', '--',
         'Engine/src/Scripting/'],
        capture_output=True, text=True, cwd=ROOT).stdout
    for line in out.splitlines():
        m = re.search(r'([A-Za-z_]+_[A-Za-z_0-9]+)\($', line.strip())
        if m:
            names.add(m.group(1))

    api_dir = os.path.join(ROOT, 'enjin_api')
    for dirpath, _, files in os.walk(api_dir):
        for f in files:
            if f.endswith('.as'):
                src = open(os.path.join(dirpath, f), encoding='utf-8',
                           errors='replace').read()
                names.update(re.findall(r'\b([A-Za-z_]+_[A-Za-z_0-9]+)\s*\(', src))
    return names


def main():
    registered = registered_names()
    if len(registered) < 200:
        print(f'only {len(registered)} bindings found - is this being run from the repo?')
        return 2

    missing = collections.defaultdict(list)
    for doc in DOCS:
        path = os.path.join(ROOT, doc)
        if not os.path.exists(path):
            continue
        text = open(path, encoding='utf-8', errors='replace').read()
        for block in re.findall(r'```(?:angelscript|as|cpp)?\n(.*?)```', text, re.S):
            for name in re.findall(r'\b([A-Za-z_]+_[A-Za-z_0-9]+)\s*\(', block):
                if PREFIXES.match(name) and name not in registered:
                    if name not in missing[doc]:
                        missing[doc].append(name)

    if not missing:
        print(f'doc API check: OK ({len(registered)} bindings, '
              f'{len(DOCS)} docs, every sampled name exists)')
        return 0

    total = 0
    for doc, names in sorted(missing.items()):
        print(f'{doc}:')
        for n in sorted(names):
            print(f'    {n} -- named in a code sample, not registered by the engine')
            total += 1
    print(f'\ndoc API check FAILED: {total} name(s) a reader cannot call')
    return 1


if __name__ == '__main__':
    sys.exit(main())
