#!/usr/bin/env python3
"""Find wired rows where a game's <waitkey> is eating solution lines.

SCARE's ANSI build answers a `<waitkey>` by reading a line from stdin.  With a
pipe that line is the next line of the solution, so it is silently swallowed:
the run does not fail, it just executes a shorter route.  Rows that set
SCR_SKIP_WAITKEY=1 are immune.

Most of this corpus handles the tag the other way, by feeding it a deliberate
filler -- a blank line (`donuts_intro`, and 77 of them in `wes_ghn`) or a
throwaway command (`farfromhome` opens with `x`).  That is fine.  What this
script looks for is the case where there is no filler to spare and a real
command goes missing.

Written 2026-08-12 while wiring Salutations, which is the clean demonstration:
it still WINS with its first command eaten, so blessing it without
SCR_SKIP_WAITKEY=1 would have recorded a shifted transcript rather than
failing.  See notes/WALKTHROUGH_TODO.md, "Salutations".

    python3 waitkey_audit.py            # summary + the suspect list
    python3 waitkey_audit.py -v         # list every row in every bucket

Buckets:
  IMMUNE     row already sets SCR_SKIP_WAITKEY=1
  NO-WAITKEY the .taf has no <waitkey> at all
  ABSORBED   <waitkey> present but SCR_SKIP_WAITKEY=1 changes nothing -- the
             tag is never reached on this route
  FILLED     lines are swallowed, but the solution has at least that many
             blank lines, i.e. the filler convention is doing its job
  SUSPECT    more lines swallowed than the solution has blanks to spare, so at
             least one is probably a real command.  `icecream` is the proven
             case: `take cone` never runs, and the golden records it.
             SUSPECT is a flag, not a verdict -- a non-blank filler such as
             `farfromhome`'s leading `x` lands here too.  Check the row by
             hand before touching it; adding SCR_SKIP_WAITKEY=1 rewrites the
             golden and can invalidate a route that was derived under the
             shift.
"""

import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from taf_pattern_scan import plaintext                      # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
GAMES = os.path.join(HERE, '..', 'games')
GOLDENS = os.path.join(HERE, '..', 'goldens')
SCARE = os.path.join(HERE, 'scare')
MAP = os.path.join(HERE, 'run_v4_walkthroughs.sh')


def rows():
    for line in open(MAP, encoding='utf-8', errors='replace'):
        if line.startswith('#') or '|' not in line:
            continue
        p = line.rstrip('\n').split('|')
        if p[0].endswith('_solution.txt'):
            yield p


def row_env(p):
    env = {}
    for field in p[3:]:
        for assign in field.split():
            if '=' in assign:
                k, v = assign.split('=', 1)
                env[k] = v
    return env


def prompts(game, sol, env):
    """Number of `>` input prompts the run gets through, or None on timeout."""
    e = dict(os.environ)
    e['LC_ALL'] = 'C'
    e.update(env)
    data = open(sol, 'rb').read() + b'quit\ny\n'
    try:
        out = subprocess.run([SCARE, game], input=data, capture_output=True,
                             timeout=60, env=e).stdout
    except subprocess.TimeoutExpired:
        return None
    return out.count(b'\n>')


def blank_lines(sol):
    return sum(1 for line in open(sol, encoding='utf-8', errors='replace')
               if not line.startswith('#') and not line.strip())


def main():
    verbose = '-v' in sys.argv
    buckets = {k: [] for k in
               ('IMMUNE', 'NO-WAITKEY', 'ABSORBED', 'FILLED', 'SUSPECT')}
    for p in rows():
        game = os.path.join(GAMES, p[1])
        sol = os.path.join(GOLDENS, p[0])
        if not os.path.exists(game) or not os.path.exists(sol):
            continue
        env = row_env(p)
        if 'SCR_SKIP_WAITKEY' in env:
            buckets['IMMUNE'].append(p[0])
            continue
        data, _ver = plaintext(game)
        if b'<waitkey>' not in data.lower():
            buckets['NO-WAITKEY'].append(p[0])
            continue
        plain = prompts(game, sol, env)
        skipped = prompts(game, sol, dict(env, SCR_SKIP_WAITKEY='1'))
        if plain is None or skipped is None or plain == skipped:
            buckets['ABSORBED'].append(p[0])
            continue
        eaten, blanks = skipped - plain, blank_lines(sol)
        label = '%s (%+d lines, %d blank)' % (p[0], eaten, blanks)
        buckets['FILLED' if eaten <= blanks else 'SUSPECT'].append(label)

    for name in ('IMMUNE', 'NO-WAITKEY', 'ABSORBED', 'FILLED', 'SUSPECT'):
        print('%-11s %d' % (name, len(buckets[name])))
        if verbose or name == 'SUSPECT':
            for row in buckets[name]:
                print('    ', row)
    return 0


if __name__ == '__main__':
    sys.exit(main())
