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


# Types a sample may name. Anything else in a type position is either a class the
# sample declares itself or a mistake, so this is a DENYLIST of known-wrong
# spellings rather than a guess at every legal type. Each entry says what to write
# instead, because "Vec3 is not a type" is a worse error than "Vec3 -> Vector3".
WRONG_TYPES = {
    'Vec2': 'Vector2',
    'Vec3': 'Vector3',
    'Vec4': 'Vector4',
    'Quat': 'Quaternion',
}

# Identifiers a sample assumes are in scope and are not. `self` is the big one:
# 55 uses across the tutorial book, and no script has ever had it. A script class
# derives from TegeBehavior, which exposes the entity id through GetEntity().
WRONG_IDENTS = {
    'self': 'GetEntity() (TegeBehavior exposes the entity id through a method)',
}


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


# Walk the fences line by line instead of pairing them with a regex.
#
# The regex form was `r'```(?:angelscript|as|cpp)?\n(.*?)```'`, which does not
# match a fence in any OTHER language -- ```text, ```json, ```mermaid, ```bash.
# The engine then treats that block's CLOSING fence as an opening one, and every
# fence after it in the file is paired to the wrong partner: prose gets scanned as
# code (a "self" in "self-organizing" reported as a wrong identifier) and real
# code gets skipped as prose, which is the half that matters -- a sample calling a
# function that does not exist would pass unseen. Pairing has to be done by
# walking, because only the opening fence carries the language.
CODE_LANGS = ('angelscript', 'as', 'cpp')


def code_blocks(text):
    blocks = []
    lang = None
    body = []
    for line in text.split('\n'):
        # Markdown allows an indented fence (inside a list item), and an indented
        # pair this walker did not recognise would leave it in whatever state it
        # was already in -- the same desync, one level down.
        stripped = line.strip()
        if lang is None:
            if stripped.startswith('```'):
                lang = stripped[3:].strip().lower()
                body = []
            continue
        if stripped == '```':
            if lang in CODE_LANGS:
                blocks.append('\n'.join(body))
            lang = None
            continue
        body.append(line)
    return blocks


# The other direction: how much of the API the reference actually mentions.
#
# The check above asks whether every name a DOC uses exists. This asks whether
# every name that EXISTS is mentioned by the reference, which is the question a
# person has when a binding works and they cannot find out that it is there.
# Measured 2026-09-16: 1249 registered, 621 named, 628 undocumented -- about half
# the scripting API, while SCRIPTING_API.md presents itself as the reference.
#
# Writing 628 entries is not a thing a check can do. Keeping the gap from growing
# is: --strict fails only when the undocumented count goes ABOVE the recorded
# baseline, so adding a binding without documenting it is caught while the
# existing backlog is not treated as a new failure.
COVERAGE_BASELINE = os.path.join(ROOT, 'tools', 'doc_api_coverage_baseline.txt')


def coverage(registered):
    ref = os.path.join(ROOT, 'docs/SCRIPTING_API.md')
    if not os.path.exists(ref):
        return None, None
    text = open(ref, encoding='utf-8', errors='replace').read()
    named = {n for n in registered if n in text}
    return len(named), sorted(registered - named)


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
        for block in code_blocks(text):
            # Strip line comments so prose inside a sample cannot trip the check.
            code = re.sub(r'//[^\n]*', '', block)

            for name in re.findall(r'\b([A-Za-z_]+_[A-Za-z_0-9]+)\s*\(', code):
                if PREFIXES.match(name) and name not in registered:
                    entry = f'{name}() -- named in a code sample, not registered'
                    if entry not in missing[doc]:
                        missing[doc].append(entry)

            for wrong, right in WRONG_TYPES.items():
                if re.search(r'\b' + wrong + r'\b', code):
                    entry = f'{wrong} -- not a registered type; use {right}'
                    if entry not in missing[doc]:
                        missing[doc].append(entry)

            for wrong, right in WRONG_IDENTS.items():
                if re.search(r'\b' + wrong + r'\b', code):
                    entry = f'{wrong} -- not in scope in a script; use {right}'
                    if entry not in missing[doc]:
                        missing[doc].append(entry)

    named, undocumented = coverage(registered)
    cov_failed = False
    if named is not None:
        print(f'reference coverage: {named}/{len(registered)} bindings named in '
              f'SCRIPTING_API.md, {len(undocumented)} undocumented')
        baseline = None
        if os.path.exists(COVERAGE_BASELINE):
            for line in open(COVERAGE_BASELINE, encoding='utf-8'):
                line = line.split('#')[0].strip()
                if line.isdigit():
                    baseline = int(line)
                    break
        if baseline is not None and len(undocumented) > baseline:
            print(f'  UNDOCUMENTED COUNT ROSE: {len(undocumented)} > baseline {baseline}')
            print('  A new binding was registered without a line in SCRIPTING_API.md.')
            print('  Document it, or lower the baseline deliberately if entries were removed.')
            for n in undocumented[:10]:
                print(f'    {n}')
            cov_failed = True
        elif baseline is not None and len(undocumented) < baseline:
            print(f'  (baseline {baseline} can be lowered to {len(undocumented)})')

    if not missing:
        print(f'doc API check: OK ({len(registered)} bindings, '
              f'{len(DOCS)} docs, every sampled name exists)')
        return 1 if (cov_failed and '--strict' in sys.argv) else 0

    total = 0
    for doc, names in sorted(missing.items()):
        print(f'{doc}:')
        for n in sorted(names):
            print(f'    {n}')
            total += 1
    print(f'\ndoc API check FAILED: {total} name(s) a reader cannot call')
    return 1


if __name__ == '__main__':
    sys.exit(main())
