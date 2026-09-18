#!/usr/bin/env python3
"""Authored component fields that nothing reads.

A field with an inspector row, a serializer entry and no reader is a setting
that saves, reloads, survives a round trip, and does nothing. That is the
sharpest shape of the silent-stub problem this repo keeps finding: everything
about it looks present except the part that matters, and no test catches it
because the round trip genuinely works.

The question asked here is deliberately narrow and mechanical:

    for each field of each *Component struct, does ANY file outside the
    component's own header, the scene serializer, and the editor's inspector
    mention it?

Those three make a field storable and editable, not effective.

What this CANNOT tell you, and the reason a clean report is not a guarantee: a
value read into a local that nothing then uses passes unchanged. "Has a reader"
is weaker than "works". It is still the difference between a field that might
work and one that provably cannot.

    python tools/unread_field_audit.py              # report
    python tools/unread_field_audit.py --strict     # exit 1 on any finding
    python tools/unread_field_audit.py --component MeshRendererComponent
"""
import argparse
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

COMPONENT_DIRS = [
    os.path.join('Engine', 'include', 'Enjin', 'ECS', 'Components'),
]
SEARCH_DIRS = [
    os.path.join('Engine', 'src'),
    os.path.join('Engine', 'include'),
    os.path.join('Player', 'src'),
    os.path.join('Editor', 'src'),
]

# Storable and editable is not the same as effective.
EXCLUDE_SUFFIXES = (
    'SceneSerializer.cpp',
    'EditorLayerComponents.cpp',
    'EditorLayerInspector.cpp',
)

STRUCT = re.compile(r'struct\s+(?:ENJIN_API\s+)?([A-Za-z_]\w*Component)\s*(?:final\s*)?\{')
FIELD = re.compile(r'^\s+(?:mutable\s+)?[A-Za-z_][\w:]*(?:<[^>]*>)?\s+([A-Za-z_]\w*)\s*(?:=|;)')

# Names common enough that a grep for them proves nothing either way.
TOO_GENERIC = {
    'x', 'y', 'z', 'w', 'r', 'g', 'b', 'a', 'id', 'name', 'type', 'mode',
    'value', 'count', 'size', 'index', 'enabled', 'visible', 'color', 'colour',
    'position', 'rotation', 'scale', 'path', 'time', 'speed', 'offset', 'data',
}


def strip_comments(text):
    text = re.sub(r'/\*.*?\*/', '', text, flags=re.S)
    return '\n'.join(line.split('//')[0] for line in text.split('\n'))


def components_in(path):
    """{struct name: [field, ...]} for every *Component in one header."""
    text = strip_comments(open(path, encoding='utf-8', errors='replace').read())
    lines = text.split('\n')
    out = {}
    i = 0
    while i < len(lines):
        m = STRUCT.search(lines[i])
        if not m:
            i += 1
            continue
        name = m.group(1)
        depth = lines[i].count('{') - lines[i].count('}')
        fields = []
        i += 1
        while i < len(lines) and depth > 0:
            line = lines[i]
            depth += line.count('{') - line.count('}')
            # Depth 1 only: a nested struct's fields belong to the nested type.
            if depth == 1:
                fm = FIELD.match(line)
                if fm and not re.match(r'^\s*(return|if|for|while|using|typedef)\b', line):
                    fields.append(fm.group(1))
            i += 1
        if fields:
            out.setdefault(name, fields)
    return out


def readers(field):
    dirs = [os.path.join(ROOT, d) for d in SEARCH_DIRS if os.path.isdir(os.path.join(ROOT, d))]
    try:
        r = subprocess.run(['grep', '-rl', r'\b' + field + r'\b'] + dirs,
                           capture_output=True, text=True, timeout=180)
    except Exception:
        return None   # unknown, not "none"
    files = [f for f in r.stdout.split('\n') if f.strip()]
    return [f for f in files
            if not any(f.endswith(s) for s in EXCLUDE_SUFFIXES)
            and not f.endswith('.h') or 'Components' not in f]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--strict', action='store_true')
    ap.add_argument('--component', help='audit one component instead of all')
    args = ap.parse_args()

    comps = {}
    for d in COMPONENT_DIRS:
        base = os.path.join(ROOT, d)
        if not os.path.isdir(base):
            continue
        for name in sorted(os.listdir(base)):
            if name.endswith('.h'):
                comps.update(components_in(os.path.join(base, name)))

    if args.component:
        comps = {k: v for k, v in comps.items() if k == args.component}
        if not comps:
            print('no such component')
            return 1

    findings = []
    checked = 0
    for comp in sorted(comps):
        for field in comps[comp]:
            if field in TOO_GENERIC or field.startswith('cached') or field.startswith('m_'):
                continue
            checked += 1
            rs = readers(field)
            if rs is not None and not rs:
                findings.append((comp, field))

    print('Unread field audit')
    print('  %d components, %d fields checked' % (len(comps), checked))
    print('  (skipped: names too generic to grep, cached* runtime state)')

    if findings:
        print('\nNO READER outside the header, the serializer and the inspector:')
        for comp, field in findings:
            print('  %s.%s' % (comp, field))
        print('\n%d field(s). Each is either dead, or read somewhere this cannot see --'
              % len(findings))
        print('check before deleting: a read through a macro or a generated')
        print('accessor does not grep like a normal one.')
        return 1 if args.strict else 0

    print('\nEvery checked field is mentioned somewhere that could use it.')
    print('That is weaker than "works": a value read into a local that nothing')
    print('uses passes this unchanged.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
