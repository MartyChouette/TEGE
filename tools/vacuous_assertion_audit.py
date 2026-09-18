#!/usr/bin/env python3
"""Find assertions that cannot fail.

The framework already refuses to pass a test that asserted nothing -- see the
`ctx.assertions == 0` guard in Tests/Framework/EnjinTest.h. That guard counts
assertions; it cannot look at them. So `ENJIN_EXPECT_TRUE(true)` satisfies it
while checking nothing, and the pattern the guard was built to stop comes back
in a form the guard cannot see. ENJIN_SKIP (reports SKIP with a reason, never a
pass) and ENJIN_SURVIVED (counts, and records WHAT was survived) exist for the
two honest versions of this, and both are what should be written instead.

Two shapes are reported:

  literal   ENJIN_*_TRUE(true) / ENJIN_*_FALSE(false) -- an assertion whose
            operand is the constant that makes it PASS. The mirror image,
            ENJIN_EXPECT_TRUE(false) in a branch that must not be reached, is
            a deliberate fail-if-reached and is not reported: it can fail,
            which is the whole test.
  identical ENJIN_*_EQ(x, x) where both sides are textually the same expression.
            Two calls with the same arguments agree in every implementation,
            right or wrong, so the comparison pins nothing.

Tests/Unit/Core/TestTestFramework.cpp is exempt: it tests the assertion macros
themselves, and there a literal operand is the subject rather than a shortcut.

  python tools/vacuous_assertion_audit.py           # list findings
  python tools/vacuous_assertion_audit.py --strict  # exit 1 if any exist
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TESTS = os.path.join(ROOT, 'Tests')

# The one file where a literal operand is the thing under test.
EXEMPT = {os.path.normcase(os.path.join('Tests', 'Unit', 'Core', 'TestTestFramework.cpp'))}

# Only the pairings that ALWAYS PASS. EXPECT_TRUE(false) always fails, so it is
# an assertion doing its job from inside a branch that should be unreachable.
ALWAYS_PASSES = {('TRUE', 'true'), ('TRUE', '1'), ('FALSE', 'false'), ('FALSE', '0')}

LITERAL_RE = re.compile(
    r'\bENJIN_(?:EXPECT|ASSERT)_(TRUE|FALSE)\s*\(\s*([A-Za-z0-9_]+)\s*\)')

# EQ/NE with two top-level arguments. Nesting makes a regex the wrong tool for
# splitting, so the split is done by scanning parens.
CMP_RE = re.compile(r'\bENJIN_(?:EXPECT|ASSERT)_(?:EQ|NE|STR_EQ|FLOAT_EQ)\s*\(')


def split_args(text, start):
    """Given text and the index just past '(', return (args, end) or None."""
    depth, arg, args = 1, [], []
    i = start
    while i < len(text):
        c = text[i]
        if c in '([{':
            depth += 1
        elif c in ')]}':
            depth -= 1
            if depth == 0:
                args.append(''.join(arg))
                return args, i
        if depth == 1 and c == ',':
            args.append(''.join(arg))
            arg = []
        else:
            arg.append(c)
        i += 1
    return None


def normalise(expr):
    return re.sub(r'\s+', '', expr)


def scan(path, rel):
    findings = []
    with open(path, encoding='utf-8', errors='replace') as fh:
        source = fh.read()
    lines = source.split('\n')

    for n, line in enumerate(lines, 1):
        stripped = line.lstrip()
        if stripped.startswith('//'):
            continue
        m = LITERAL_RE.search(line)
        if m and (m.group(1), m.group(2)) in ALWAYS_PASSES:
            findings.append((rel, n, 'literal', line.strip()))

    # Comparisons can span lines, so scan the whole source for these.
    for m in CMP_RE.finditer(source):
        got = split_args(source, m.end())
        if not got:
            continue
        args, end = got
        if len(args) < 2:
            continue
        if normalise(args[0]) == normalise(args[1]):
            n = source.count('\n', 0, m.start()) + 1
            # Skip a commented-out call.
            line_start = source.rfind('\n', 0, m.start()) + 1
            if source[line_start:m.start()].lstrip().startswith('//'):
                continue
            findings.append((rel, n, 'identical',
                             re.sub(r'\s+', ' ', source[m.start():end + 1])))
    return findings


def main():
    strict = '--strict' in sys.argv
    findings = []
    for base, _dirs, files in os.walk(TESTS):
        for name in files:
            if not name.endswith(('.cpp', '.h')):
                continue
            path = os.path.join(base, name)
            rel = os.path.relpath(path, ROOT)
            if os.path.normcase(rel) in EXEMPT:
                continue
            findings.extend(scan(path, rel))

    if not findings:
        print('vacuous assertion audit: none found')
        return 0

    print('Assertions that cannot fail:\n')
    for rel, line, kind, text in sorted(findings):
        print('  %-12s %s:%d' % (kind, rel.replace(chr(92), '/'), line))
        print('               %s' % text)
    print('\n%d found. ENJIN_SKIP(reason) for "there is nothing to test here",'
          '\nENJIN_SURVIVED(what) for "reaching this line IS the property".' % len(findings))
    return 1 if strict else 0


if __name__ == '__main__':
    sys.exit(main())
