#!/usr/bin/env python3
"""Summarise replay.py's output by row, game and version, and diff list C
against notes/SCARIER_vs_SCARE.md.

Usage: analyze.py RESULTS_TSV
"""
import collections, os, re, sys

T = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__)))))
G = T + '/test/adrift4/games/'
NOTE = T + '/notes/SCARIER_vs_SCARE.md'
SIG = {'93453e61': '4.0', '94453761': '3.9', '94453661': '3.8', '94453961': '3.7'}

titles = {}
for line in open(T + '/test/adrift4/games.manifest.tsv', encoding='utf-8', errors='replace'):
    f = line.rstrip('\n').split('\t')
    if len(f) > 5 and not f[0].startswith('#'):
        titles[f[0]] = f[5]


def version(game):
    try:
        return SIG.get(open(G + game, 'rb').read(14)[8:12].hex(), '?')
    except OSError:
        return '?'


rows = [l.rstrip('\n').split('\t') for l in open(sys.argv[1], encoding='latin-1')]
rc = collections.Counter(r[2] for r in rows)
print('rows', len(rows), dict(rc))

games = collections.defaultdict(list)
for r in rows:
    games[r[1]].append(r)
state = {}
for g, rs in games.items():
    sts = {r[2] for r in rs}
    if 'WIN' in sts:
        state[g] = 'WIN'
    elif 'NOLOAD' in sts:
        state[g] = 'NOLOAD'
    elif 'ABORT' in sts:
        state[g] = 'ABORT'
    else:
        state[g] = 'NOWIN'
print('games', len(state), dict(collections.Counter(state.values())))
print('rows by game state:')
for st in ('NOWIN', 'ABORT', 'NOLOAD'):
    n = sum(1 for r in rows if r[2] == st)
    print(' ', st, 'rows', n, 'games', sum(1 for v in state.values() if v == st))
vt = collections.defaultdict(collections.Counter)
for g, st in state.items():
    vt[version(g)][st] += 1
for v in sorted(vt, reverse=True):
    print(' ', v, sum(vt[v].values()), dict(vt[v]))

for st in ('NOLOAD', 'ABORT'):
    print('==', st)
    for g in sorted(k for k, v in state.items() if v == st):
        e = next(r[4] for r in games[g] if r[2] == st)
        print('  ', titles.get(g, g), '|', g, '|', version(g), '|', e)

# Diff list C against the note.
note = open(NOTE, encoding='utf-8').read()
old_c = set()
for m in re.finditer(r'### C\. Route does not reach its ending[^\n]*\n\n([^\n]+)', note):
    for name in m.group(1).split('; '):
        old_c.add(re.sub(r' \[[A-Z,]+\]$', '', name.strip()))
new_c = {titles.get(g, g): g for g, st in state.items() if st == 'NOWIN'}
print('== list C now', len(new_c), 'was', len(old_c))
print('  left C:', sorted(old_c - set(new_c)))
print('  joined C:', sorted(set(new_c) - old_c))
