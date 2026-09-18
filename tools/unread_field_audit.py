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

NOT the same question as tools/field_coverage.py, which sits beside it. That
one asks whether a field SAVES and can be AUTHORED -- is it in the serializer,
the deserializer, the inspector. This one asks whether anything CONSUMES it. A
field can pass either and fail the other, and the two failures look nothing
alike: a field that does not save loses its value on reload, a field nothing
reads holds its value forever and changes nothing.

What this CANNOT tell you, and the reason a clean report is not a guarantee: a
value read into a local that nothing then uses passes unchanged. "Has a reader"
is weaker than "works". It is still the difference between a field that might
work and one that provably cannot.

    python tools/unread_field_audit.py                   # report
    python tools/unread_field_audit.py --strict          # exit 1 if worse than the baseline
    python tools/unread_field_audit.py --write-baseline  # record where it stands
    python tools/unread_field_audit.py --component MeshRendererComponent
"""
import argparse
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BASELINE = os.path.join(ROOT, 'tools', 'unread_field_baseline.txt')

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

    # A component header is dropped wholesale below, on the grounds that the only
    # mention of a field in it is the field's own declaration. That is wrong for a
    # component with INLINE METHODS: ResourceComponent::Regenerate genuinely reads
    # timeSinceLastUse, regenDelay and depletedThreshold, in the header, and all
    # three were reported as unread. A false positive costs more than it looks --
    # the triage goes and wires a field that already works, which means editing
    # correct code and writing a test for behaviour that was already there.
    #
    # So the header counts as a reader when the field appears on a line that is
    # NOT its declaration: a use inside a method body.
    def header_uses_beyond_declaration(path):
        try:
            with open(path, encoding='utf-8', errors='replace') as fh:
                lines = fh.read().split('\n')
        except OSError:
            return False
        use = re.compile(r'\b' + re.escape(field) + r'\b')
        # "f32 depletedThreshold = 20.0f;" -- a type, the name, then an
        # initialiser or a terminator. Anything else mentioning it is a use.
        decl = re.compile(r'^[A-Za-z_][\w:<>,\s\*&]*\b' + re.escape(field) + r'\b\s*(=|;|\{)')
        for line in lines:
            stripped = line.strip()
            if not use.search(stripped) or stripped.startswith('//'):
                continue
            if decl.match(stripped):
                continue
            return True
        return False
    # Parenthesised deliberately. Written without them this read
    #     (not excluded and not a component header) or 'Components' not in f
    # because `and` binds tighter than `or`, so EVERY path without "Components"
    # in it counted as a reader -- including SceneSerializer.cpp, the file this
    # is supposed to ignore. The audit therefore only found fields that were
    # missing from the serializer as well, and reported a clean bill for the
    # much larger class it exists to catch.
    def is_reader(f):
        if any(f.endswith(s) for s in EXCLUDE_SUFFIXES):
            return False                       # storable/editable, not effective
        if f.endswith('.h') and 'Components' in f:
            # Its declaration only, unless a method in the same header uses it.
            return header_uses_beyond_declaration(f)
        return True

    return [f for f in files if is_reader(f)]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--strict', action='store_true',
                    help='exit 1 if any field is unread that was not in the baseline')
    ap.add_argument('--write-baseline', action='store_true',
                    help='record the current findings as accepted')
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

    names = sorted('%s.%s' % (c, f) for c, f in findings)

    if args.write_baseline:
        with open(BASELINE, 'w', encoding='utf-8') as f:
            f.write('# Recorded by tools/unread_field_audit.py. These are the fields that\n'
                    '# nothing reads TODAY, accepted so a new one fails CI on its own.\n'
                    '# Each line is either dead weight or a field read somewhere the grep\n'
                    '# cannot see. Removing one and re-recording is the only way down.\n')
            for n in names:
                f.write(n + '\n')
        print('\nbaseline written: %d field(s)' % len(names))
        return 0

    if findings:
        print('\nNO READER outside the header, the serializer and the inspector:')
        for n in names:
            print('  ' + n)

    if args.strict:
        if not os.path.isfile(BASELINE):
            print('\nno baseline recorded -- run with --write-baseline')
            return 1
        known = set()
        for line in open(BASELINE, encoding='utf-8'):
            line = line.strip()
            if line and not line.startswith('#'):
                known.add(line)
        fresh = [n for n in names if n not in known]
        if fresh:
            print('\nNEW since the baseline:')
            for n in fresh:
                print('  ' + n)
            print('\nA field that saves, loads, shows in the inspector and is read by '
                  'nothing\nis the defect this counts. Wire it up, or add it to the '
                  'baseline deliberately.')
            return 1
        gone = sorted(known - set(names))
        if gone:
            print('\nno longer unread: %s -- re-record with --write-baseline'
                  % ', '.join(gone))
        print('\nnothing newly unread')
        return 0

    if not findings:
        print('\nEvery checked field is mentioned somewhere that could use it.')
    print('That is weaker than "works": a value read into a local that nothing')
    print('uses passes this unchanged.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
