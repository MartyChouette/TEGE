#!/usr/bin/env python3
"""Generate the complete binding index in docs/SCRIPTING_API.md.

The reference was hand-written and had drifted badly: measured 2026-09-16, 1250
registered bindings with 621 named in the document and 629 not. A person who
found a binding working had a coin-flip chance of finding a line about it, which
is the worst way for a reference to fail -- it does not look incomplete, it looks
like the thing does not exist.

Hand-writing 629 entries was not the answer either. Half of them would have been
invented prose: only 89 of the 1300-odd registrations carry a usable comment at
their definition, and several of those are section banners rather than
descriptions. A generated index of SIGNATURES is worth more than paragraphs of
guessed behaviour, because the signature is the part a caller cannot infer and
it comes straight from the registration string the engine actually uses.

So: the prose sections above the marker stay hand-written, because explaining a
concept is a person's job. Everything registered appears below it with its exact
declaration, grouped by the file it is registered in. Re-run after adding
bindings; the block between the markers is replaced wholesale.

    python tools/gen_scripting_api.py            # rewrite the index
    python tools/gen_scripting_api.py --check    # exit 1 if it is out of date
"""
import glob
import io
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DOC = os.path.join(ROOT, 'docs', 'SCRIPTING_API.md')
BEGIN = '<!-- BEGIN GENERATED BINDING INDEX -- tools/gen_scripting_api.py -->'
END = '<!-- END GENERATED BINDING INDEX -->'

# Source file -> the heading it appears under. A file with no entry here falls
# back to its own name, so a new bindings file shows up rather than vanishing.
SECTIONS = {
    'ScriptBindings.cpp': 'Core, math, debug and entity',
    'ScriptBindings_AI.cpp': 'AI and navigation',
    'ScriptBindings_Accessibility.cpp': 'Accessibility',
    'ScriptBindings_Audio.cpp': 'Audio',
    'ScriptBindings_Components.cpp': 'Components',
    'ScriptBindings_GameplayComponents.cpp': 'Gameplay components',
    'ScriptBindings_Input.cpp': 'Input',
    'ScriptBindings_MIDI.cpp': 'MIDI',
    'ScriptBindings_Physics.cpp': 'Physics',
    'ScriptBindings_Procedural.cpp': 'Procedural generation',
    'ScriptBindings_Render.cpp': 'Rendering',
    'ScriptBindings_Scene.cpp': 'Scene and save data',
    'ScriptBindings_UI.cpp': 'UI and dialogue',
    'FlashAPIShim.cpp': 'Flash API shim',
    'ScriptBindings_AudioGraph.cpp': 'Audio graph',
    'ScriptBindings_AudioReactive.cpp': 'Audio-reactive drivers',
    'ScriptBindings_Dialogue.cpp': 'Dialogue',
    'ScriptBindings_Elemental.cpp': 'Elemental effects',
    'ScriptBindings_Flower.cpp': 'Flower',
    'ScriptBindings_Gameplay.cpp': 'Gameplay systems',
    'ScriptBindings_HUD.cpp': 'HUD',
    'ScriptBindings_InputAction.cpp': 'Input actions and rebinding',
    'ScriptBindings_Networking.cpp': 'Networking (LAN)',
    'ScriptBindings_Noise.cpp': 'Noise',
    'ScriptBindings_Particles.cpp': 'Particles',
    'ScriptBindings_Plugin.cpp': 'Plugins',
    'ScriptBindings_Prefab.cpp': 'Prefabs',
    'ScriptBindings_Rewind.cpp': 'Rewind and replay',
    'ScriptBindings_Save.cpp': 'Save system',
    'ScriptBindings_Sprite.cpp': 'Sprites (2D)',
    'ScriptBindings_StateMachine.cpp': 'State machines',
    'ScriptBindings_Streaming.cpp': 'Level streaming',
    'ScriptBindings_Text.cpp': 'Text and fonts',
    'ScriptBindings_Tween.cpp': 'Tweening',
    'ScriptBindings_Water.cpp': 'Water',
    'ScriptBindings_Weather.cpp': 'Weather',
}


def declarations():
    """{section: sorted[declaration]} straight from the registration strings."""
    out = {}
    for path in sorted(glob.glob(os.path.join(ROOT, 'Engine/src/Scripting/*.cpp'))):
        base = os.path.basename(path)
        section = SECTIONS.get(base, base)
        src = io.open(path, encoding='utf-8', errors='replace').read()
        for m in re.finditer(r'RegisterGlobalFunction\(\s*"([^"]+)"', src):
            decl = ' '.join(m.group(1).split())
            if '(' not in decl:
                continue
            out.setdefault(section, set()).add(decl)
    return {k: sorted(v) for k, v in sorted(out.items())}


def render(groups):
    total = sum(len(v) for v in groups.values())
    lines = [
        BEGIN,
        '',
        '## Every registered binding',
        '',
        f'{total} global functions, grouped by where they are registered. These lines are',
        'GENERATED from the registration strings themselves, so a signature here is the',
        'one the engine accepts -- if it disagrees with the prose above, the prose is',
        'wrong. Regenerate with `python tools/gen_scripting_api.py` after adding a',
        'binding.',
        '',
        'Signatures only. Where a function needs explaining rather than listing, it is',
        'written up by hand in the sections above; this index exists so that nothing is',
        'merely absent.',
        '',
    ]
    for section, decls in groups.items():
        lines.append(f'### {section}  ({len(decls)})')
        lines.append('')
        for d in decls:
            lines.append(f'- `{d}`')
        lines.append('')
    lines.append(END)
    return '\n'.join(lines)


def main():
    groups = declarations()
    if not groups:
        print('no registrations found -- run this from the repo root')
        return 2
    block = render(groups)

    text = io.open(DOC, encoding='utf-8', errors='replace').read()
    nl = '\r\n' if text.count('\r\n') else '\n'
    body = block.replace('\n', nl)

    if BEGIN in text and END in text:
        head = text[:text.index(BEGIN)]
        tail = text[text.index(END) + len(END):]
        updated = head + body + tail
    else:
        updated = text.rstrip() + nl + nl + body + nl

    if '--check' in sys.argv:
        if updated != text:
            print('SCRIPTING_API.md binding index is out of date -- run '
                  'python tools/gen_scripting_api.py')
            return 1
        print(f'binding index up to date ({sum(len(v) for v in groups.values())} bindings)')
        return 0

    io.open(DOC, 'w', encoding='utf-8', newline='').write(updated)
    print(f'wrote {sum(len(v) for v in groups.values())} bindings across '
          f'{len(groups)} sections into docs/SCRIPTING_API.md')
    return 0


if __name__ == '__main__':
    sys.exit(main())
