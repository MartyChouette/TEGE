#!/usr/bin/env python3
"""Count bare ImGui edit widgets in the files that edit scene state.

A scene-state widget should go through InspectorUndo (one undo step per edit,
named after the field). A bare ImGui::DragFloat still changes the scene, and is
only caught by the inspector's whole-entity snapshot, or the Scene tab's
settings snapshot, where one exists. Neither names the field, and outside those
two panels a bare widget has no undo at all.

A baseline, not a target, the same shape as field_coverage.py: --strict fails
when a file's count goes UP. Wrap some and re-record with --write-baseline.

    python tools/raw_widget_lint.py                 # per-file counts
    python tools/raw_widget_lint.py --verbose       # every hit, with its line
    python tools/raw_widget_lint.py --strict        # exit 1 if any count rose
    python tools/raw_widget_lint.py --write-baseline

It reads text, not C++: a widget inside a comment or a string is skipped, and a
widget that edits editor settings rather than the scene still counts, which is
why the number is a ceiling to hold rather than a list of bugs.
"""
import argparse
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EDITOR = os.path.join(ROOT, 'Engine', 'src', 'Editor')
BASELINE = os.path.join(ROOT, 'tools', 'raw_widget_baseline.txt')

# The files whose widgets edit what a scene saves
FILES = [
    'EditorLayerComponents.cpp',
    'EditorLayerComponents_Audio.cpp',
    'EditorLayerComponents_Physics.cpp',
    'EditorLayerInspector.cpp',
    'EditorLayerRendering.cpp',
]

WIDGETS = ('DragFloat', 'DragFloat2', 'DragFloat3', 'DragFloat4', 'DragInt', 'DragInt2',
           'SliderFloat', 'SliderFloat2', 'SliderFloat3', 'SliderInt', 'SliderAngle',
           'Checkbox', 'ColorEdit3', 'ColorEdit4', 'ColorPicker3', 'ColorPicker4',
           'InputText', 'InputTextMultiline', 'InputFloat', 'InputFloat3', 'InputInt',
           'Combo', 'RadioButton', 'VSliderFloat')
WIDGET_RE = re.compile(r'\bImGui::(' + '|'.join(WIDGETS) + r')\s*\(')


def strip_comments_and_strings(src):
    out, i, n = [], 0, len(src)
    while i < n:
        c = src[i]
        if src.startswith('//', i):
            j = src.find('\n', i)
            i = n if j < 0 else j
        elif src.startswith('/*', i):
            j = src.find('*/', i + 2)
            out.append('\n' * src.count('\n', i, n if j < 0 else j))
            i = n if j < 0 else j + 2
        elif c == '"' or c == "'":
            q, i = c, i + 1
            while i < n and src[i] != q:
                i += 2 if src[i] == '\\' else 1
            i += 1
            out.append('""')
        else:
            out.append(c)
            i += 1
    return ''.join(out)


def count(path):
    with open(path, encoding='utf-8', errors='replace') as f:
        src = strip_comments_and_strings(f.read())
    hits = []
    for m in WIDGET_RE.finditer(src):
        hits.append((src.count('\n', 0, m.start()) + 1, m.group(1)))
    return hits


def load_baseline():
    base = {}
    if os.path.exists(BASELINE):
        for line in open(BASELINE, encoding='utf-8'):
            line = line.strip()
            if line and not line.startswith('#'):
                name, n = line.rsplit(' ', 1)
                base[name] = int(n)
    return base


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--verbose', action='store_true')
    ap.add_argument('--strict', action='store_true')
    ap.add_argument('--write-baseline', action='store_true')
    args = ap.parse_args()

    counts = {}
    for name in FILES:
        hits = count(os.path.join(EDITOR, name))
        counts[name] = len(hits)
        print(f'{len(hits):5d}  {name}')
        if args.verbose:
            for line, w in hits:
                print(f'         {name}:{line}  ImGui::{w}')
    print(f'{sum(counts.values()):5d}  total')

    if args.write_baseline:
        with open(BASELINE, 'w', encoding='utf-8', newline='\n') as f:
            f.write('# Recorded by tools/raw_widget_lint.py: bare ImGui edit widgets per file.\n'
                    '# --strict fails if any number goes UP. Wrap some, then re-record.\n')
            for name in FILES:
                f.write(f'{name} {counts[name]}\n')
        print('baseline written')
        return 0

    if args.strict:
        base = load_baseline()
        worse = [(n, base.get(n, 0), c) for n, c in counts.items() if c > base.get(n, 0)]
        better = [n for n, c in counts.items() if n in base and c < base[n]]
        for n, b, c in worse:
            print(f'WORSE: {n} has {c} bare widgets, baseline {b}. Use InspectorUndo:: for scene state.')
        if better:
            print('better than baseline on: ' + ', '.join(better) + ' -- re-record with --write-baseline')
        if worse:
            return 1
        print('no worse than the recorded baseline')
    return 0


if __name__ == '__main__':
    sys.exit(main())
