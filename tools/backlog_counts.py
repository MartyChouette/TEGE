#!/usr/bin/env python3
"""Recompute the per-section counts in BACKLOG.md's headings.

The headings carried hand-written totals -- "## Renderer & GPU  (99)" -- and
they drifted the moment anyone added a row. By 2026-09-19 that section held 120
rows and still said 99. This is the same defect CLAUDE.md records about the test
count that "lived on this line for months and was wrong every time anyone
checked": a number nobody can recompute is worse than no number, because it gets
quoted into commit messages and issue bodies where the caveat does not travel.

So it is computed, not maintained. Run it after editing the backlog:

    python tools/backlog_counts.py            # rewrite the headings
    python tools/backlog_counts.py --check    # non-zero if they are stale (CI)

The heading format is "## <name>  (<open> open, <done> done)".
"""
import argparse
import re
import sys

BACKLOG = '_docs_internal/BACKLOG.md'

HEADING = re.compile(r'^## (?P<name>.*?)(?:\s{2,}\(.*\))?\s*$')
OPEN_ROW = re.compile(r'^- \[ \]')
DONE_ROW = re.compile(r'^- \[x\]', re.IGNORECASE)


def retally(lines):
    """Return (new_lines, changed) with every heading's counts recomputed."""
    # First pass: which section does each row belong to, and how many of each.
    counts = {}          # heading line index -> [open, done]
    current = None
    for i, line in enumerate(lines):
        if line.startswith('## '):
            current = i
            counts.setdefault(current, [0, 0])
        elif current is not None:
            if OPEN_ROW.match(line):
                counts[current][0] += 1
            elif DONE_ROW.match(line):
                counts[current][1] += 1

    out = list(lines)
    changed = False
    for idx, (nopen, ndone) in counts.items():
        m = HEADING.match(lines[idx])
        if not m:
            continue
        name = m.group('name').strip()
        # Sections that are prose rather than work items keep a bare heading.
        if nopen == 0 and ndone == 0:
            rebuilt = '## %s' % name
        else:
            rebuilt = '## %s  (%d open, %d done)' % (name, nopen, ndone)
        if rebuilt != lines[idx]:
            out[idx] = rebuilt
            changed = True
    return out, changed


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--check', action='store_true',
                    help='report drift and exit non-zero instead of rewriting')
    ap.add_argument('--path', default=BACKLOG)
    args = ap.parse_args()

    with open(args.path, encoding='utf-8') as f:
        lines = f.read().split('\n')

    out, changed = retally(lines)

    total_open = sum(1 for l in lines if OPEN_ROW.match(l))
    total_done = sum(1 for l in lines if DONE_ROW.match(l))

    if args.check:
        if changed:
            print('backlog counts are STALE - run: python tools/backlog_counts.py')
            for a, b in zip(lines, out):
                if a != b:
                    print('  was: %s\n  now: %s' % (a, b))
            return 1
        print('backlog counts ok (%d open, %d done)' % (total_open, total_done))
        return 0

    if changed:
        with open(args.path, 'w', encoding='utf-8', newline='') as f:
            f.write('\n'.join(out))
        print('rewrote section counts')
    else:
        print('section counts already correct')
    print('%d open, %d done, %d total' % (total_open, total_done, total_open + total_done))
    return 0


if __name__ == '__main__':
    sys.exit(main())
