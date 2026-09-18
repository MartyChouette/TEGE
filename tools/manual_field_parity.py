#!/usr/bin/env python3
"""Check docs/USER_MANUAL.md's component tables against the actual structs.

The manual documents ~200 components as markdown tables of field / type /
default. Nothing checked those against the code, so they drifted: fields that
were renamed kept their old name in the table, fields that never existed stayed
documented, and defaults changed in the header without the table moving. A
person reading the manual offline -- which is the point of the manual -- cannot
tell a stale row from a true one.

This finds three things and is deliberately conservative about all three,
because a checker that cries wolf gets muted:

  PHANTOM   a table row naming a field the struct does not have
  DEFAULT   a table row whose default disagrees with the header
  UNDOC     a struct field with no row (reported only with --undocumented,
            since plenty of fields are legitimately internal)

It does NOT try to parse every C++ initialiser. A field it cannot read a
default for is skipped rather than guessed at.

    python tools/manual_field_parity.py            # report
    python tools/manual_field_parity.py --strict   # exit 1 on any finding
"""
import argparse
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MANUAL = os.path.join(ROOT, 'docs', 'USER_MANUAL.md')
HEADER_DIRS = [
    os.path.join(ROOT, 'Engine', 'include'),
    os.path.join(ROOT, 'Core', 'include'),
]

# A table row, split on pipes. WHICH column holds the default is read from the
# table's own header rather than assumed: plenty of tables are
# field / type / description with no default at all, and treating column three
# as a default there reports every description as a wrong value.
ROW = re.compile(r'^\|\s*`([A-Za-z_][A-Za-z0-9_]*)`\s*\|')
# A heading naming a component: #### FooComponent
HEADING = re.compile(r'^#+\s+`?([A-Za-z_][A-Za-z0-9_]*Component)`?\s*$')
# A struct we can check against.
STRUCT = re.compile(r'^\s*struct\s+(?:ENJIN_API\s+)?([A-Za-z_][A-Za-z0-9_]*)\s*(?:final\s*)?\{')
# A member with a default: type name = value;  (value may contain parens/braces)
# enum class AlphaMode { Opaque, Mask, Blend } alphaMode = AlphaMode::Opaque;
# A field declared with its enum inline, which the general member pattern
# cannot see -- and a field a parser cannot see is reported as a phantom, which
# is the checker accusing the manual of the checker's own gap.
INLINE_ENUM = re.compile(
    r'^\s*enum\s+(?:class\s+)?[A-Za-z_][A-Za-z0-9_]*[^{]*\{[^}]*\}\s*'
    r'([A-Za-z_][A-Za-z0-9_]*)\s*(?:=\s*(.+?))?\s*;')

MEMBER = re.compile(
    r'^\s*(?:mutable\s+)?([A-Za-z_][A-Za-z0-9_:<>,\s\*&]*?)\s+'
    r'([A-Za-z_][A-Za-z0-9_]*)\s*(?:=\s*(.+?))?\s*;\s*(?://.*)?$')


DECLARATOR = re.compile(r'([A-Za-z_][A-Za-z0-9_]*)\s*(?:=\s*([^,]+))?$')


def split_declarators(first_name, default_text):
    """One line can declare several members: u32 gridWidth = 64, gridHeight = 64;

    Treating that as one field called gridWidth with a default of
    "64, gridHeight = 64" both mis-reads the default AND makes the second
    member look undocumented -- which is exactly what it did, and it reported
    the manual's correct gridHeight row as a phantom field.
    """
    if ',' not in default_text:
        return [(first_name, default_text)]

    # Not a list of declarators at all: a constructor or brace init.
    if '(' in default_text or '{' in default_text:
        return [(first_name, default_text)]

    parts = [p.strip() for p in default_text.split(',')]
    out = [(first_name, parts[0])]
    for part in parts[1:]:
        m = DECLARATOR.match(part)
        if m:
            out.append((m.group(1), (m.group(2) or '').strip()))
    return out


def strip_comments(text):
    text = re.sub(r'/\*.*?\*/', '', text, flags=re.S)
    return '\n'.join(line.split('//')[0] for line in text.split('\n'))


def parse_structs():
    """name -> {field: default-or-None}, for every struct under the header dirs."""
    out = {}
    for base in HEADER_DIRS:
        for dirpath, _dirs, files in os.walk(base):
            for name in files:
                if not name.endswith('.h'):
                    continue
                path = os.path.join(dirpath, name)
                with open(path, encoding='utf-8', errors='replace') as handle:
                    text = strip_comments(handle.read())

                lines = text.split('\n')
                i = 0
                while i < len(lines):
                    m = STRUCT.match(lines[i])
                    if not m:
                        i += 1
                        continue
                    struct_name = m.group(1)
                    depth = lines[i].count('{') - lines[i].count('}')
                    fields = {}
                    i += 1
                    while i < len(lines) and depth > 0:
                        line = lines[i]
                        depth += line.count('{') - line.count('}')
                        # Nested structs count too, flattened into the outer
                        # component. A component like CinematicCameraComponent
                        # keeps its per-waypoint fields in a nested Waypoint
                        # struct and the manual documents them under the one
                        # heading, which is the right way to read them -- a
                        # parser that only sees depth 1 calls every one of them
                        # a phantom field.
                        if depth >= 1:
                            em = INLINE_ENUM.match(line)
                            if em:
                                if depth == 1:
                                    fields[em.group(1)] = (em.group(2) or '').strip()
                                else:
                                    fields.setdefault(em.group(1), (em.group(2) or '').strip())
                                i += 1
                                continue
                            fm = MEMBER.match(line)
                            if fm and '(' not in fm.group(2):
                                decl_type = fm.group(1).strip()
                                # Skip functions, typedefs, statics of no interest.
                                if decl_type and not decl_type.startswith(('using', 'typedef', 'return')):
                                    for fname, fdefault in split_declarators(
                                            fm.group(2), (fm.group(3) or '').strip()):
                                        # The component's OWN field wins over a
                                        # same-named one in a nested struct --
                                        # MaterialComponent has a baseColor and
                                        # so does a texture slot inside it, and
                                        # comparing the manual against the wrong
                                        # one invents a disagreement.
                                        if depth == 1:
                                            fields[fname] = fdefault
                                        else:
                                            fields.setdefault(fname, fdefault)
                        i += 1
                    # A later definition of the same name wins nothing; first is enough.
                    out.setdefault(struct_name, fields)
    return out


NUMBER = re.compile(r'-?\d+(?:\.\d+)?(?:e-?\d+)?f?')


def normalise(value):
    """Compare defaults loosely, so only real disagreements are reported.

    1.0f, 1.0, 1 and `1.0` are one value. So are Math::Vector3(1,1,1) and
    (1, 1, 1): the manual writes the tuple a person would, the header writes
    the constructor. So are LightType::Point and Point -- a table that
    qualified every enum would be unreadable.
    """
    if value is None:
        return None
    v = value.strip().strip('`').strip().rstrip('.')
    if not v:
        return None

    low = v.lower()
    if low in ('true', 'false'):
        return low

    # A single-argument vector fills every component: Math::Vector3(0.0f) and
    # (0, 0, 0) are the same value written two ways. Checked BEFORE the general
    # bracket rule below, which would otherwise reduce it to a single number.
    m = re.match(r'^math::vector([234])\(\s*(-?[\d.]+)f?\s*\)$', low)
    if m:
        return ','.join(['%g' % float(m.group(2))] * int(m.group(1)))

    # A constructor or brace init collapses to its numbers, which is what the
    # manual's tuple form is too. Read from the first bracket onwards, or
    # Math::Vector3(1,1,1) contributes the 3 in its own type name and compares
    # unequal to (1,1,1) for a reason that takes ten minutes to spot.
    if '(' in v or '{' in v or ',' in v:
        body = v
        for opener in ('(', '{'):
            if opener in body:
                body = body[body.index(opener):]
                break
        nums = NUMBER.findall(body)
        if nums:
            return ','.join('%g' % float(n.rstrip('f')) for n in nums)

    # A simple quotient, so 1/30 and 1.0f / 30.0f agree.
    m = re.match(r'^(-?[\d.]+)f?\s*/\s*(-?[\d.]+)f?$', low)
    if m and float(m.group(2)) != 0.0:
        return '%g' % (float(m.group(1)) / float(m.group(2)))

    # Named constants whose numeric value the manual is equally right to print.
    # Checked BEFORE the enum rule below, which would otherwise reduce
    # RenderLayer::Default to the word "default" and never reach here.
    CONSTANTS = {
        'invalid_entity': '0',
        'renderlayer::default': '1',
    }
    if low in CONSTANTS:
        return CONSTANTS[low]

    # "-1 (none)" -- the value with a word for the reader. Take the value.
    m = re.match(r'^(-?[\d.]+)\s*\(.*\)$', low)
    if m:
        return '%g' % float(m.group(1))

    # A cast around a named constant.
    m = re.match(r'^static_cast<[^>]+>\((.+)\)$', v.strip())
    if m:
        return normalise(m.group(1))

    # Enum or constant: the manual names the value, the header qualifies it.
    if '::' in v:
        return v.split('::')[-1].rstrip('()').lower()

    try:
        return '%g' % float(v.rstrip('f'))
    except ValueError:
        pass

    # Spellings of the same thing that a table is right to shorten.
    aliases = {
        # INVALID_ENTITY is 0 (Entity.h) and RenderLayer::Default is 1 << 0, so
        # a table naming either is correct and a table spelling the number is
        # equally correct. Neither is drift.
        'invalid': '0',
        # The manual used to call ActionEffect::None "Nothing".
        'nothing': 'none',
        'identity': 'identity',
        'scene default': 'scenedefault',
    }
    low = low.strip('"')
    return aliases.get(low, low.replace(' ', ''))


def split_row(line):
    parts = line.strip().strip('|').split('|')
    return [p.strip() for p in parts]


def parse_manual():
    """[(component, field, documented default or None, line number)]"""
    rows = []
    with open(MANUAL, encoding='utf-8') as handle:
        lines = handle.read().split('\n')
    current = None
    default_col = None
    for n, line in enumerate(lines, 1):
        h = HEADING.match(line.strip())
        if h:
            current = h.group(1)
            default_col = None
            continue
        if not current:
            continue
        if line.startswith('#'):
            current = None
            default_col = None
            continue

        # A table header tells us where the defaults live, if anywhere. Any
        # header row, not just one that says "Field": the manual also heads
        # tables with Flag, Property and Setting, and inheriting the previous
        # table's column index reports that table's descriptions as defaults.
        stripped = line.strip()
        if stripped.startswith('|') and '`' not in stripped and '---' not in stripped:
            cols = [c.lower() for c in split_row(line)]
            default_col = cols.index('default') if 'default' in cols else None
            continue

        if not ROW.match(line):
            continue
        cells = split_row(line)
        field = cells[0].strip('`')
        doc_default = None
        if default_col is not None and default_col < len(cells):
            doc_default = cells[default_col]
        rows.append((current, field, doc_default, n))
    return rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--strict', action='store_true', help='exit 1 on any finding')
    ap.add_argument('--undocumented', action='store_true',
                    help='also list struct fields with no table row')
    args = ap.parse_args()

    structs = parse_structs()
    rows = parse_manual()

    phantom, wrong, checked, skipped_components = [], [], 0, set()
    documented = {}

    for component, field, doc_default, line in rows:
        fields = structs.get(component)
        if fields is None:
            skipped_components.add(component)
            continue
        documented.setdefault(component, set()).add(field)
        if field not in fields:
            phantom.append((component, field, line))
            continue
        code_default = fields[field]
        if not code_default or not doc_default or doc_default in ('-', '--', '', 'n/a'):
            continue
        checked += 1
        if normalise(code_default) != normalise(doc_default):
            wrong.append((component, field, doc_default, code_default, line))

    print('Manual field parity')
    print('  %d table rows across %d documented components'
          % (len(rows), len({r[0] for r in rows})))
    print('  %d defaults compared' % checked)
    if skipped_components:
        print('  %d components in the manual have no struct found (not checked)'
              % len(skipped_components))

    if phantom:
        print('\nPHANTOM fields -- documented, not in the struct:')
        for component, field, line in phantom:
            print('  USER_MANUAL.md:%d  %s.%s' % (line, component, field))

    if wrong:
        print('\nWRONG defaults -- manual disagrees with the header:')
        for component, field, doc, code, line in wrong:
            print('  USER_MANUAL.md:%d  %s.%s  manual=%s  code=%s'
                  % (line, component, field, doc, code))

    if args.undocumented:
        print('\nUNDOCUMENTED fields:')
        for component, fields in sorted(structs.items()):
            if component not in documented:
                continue
            missing = [f for f in fields if f not in documented[component]]
            if missing:
                print('  %s: %s' % (component, ', '.join(sorted(missing))))

    if not phantom and not wrong:
        print('\nNo phantom fields and no wrong defaults.')
        return 0
    print('\n%d phantom, %d wrong.' % (len(phantom), len(wrong)))
    return 1 if args.strict else 0


if __name__ == '__main__':
    sys.exit(main())
