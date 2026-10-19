#!/usr/bin/env python3
"""Replay every win-marker row of run_v4_walkthroughs.sh through stock SCARE.

Usage: replay.py SCARE_BIN OUT_TSV [substring]

Each row gets up to five seeds (its own SCR_SEED first) and is credited if any
one reaches the win marker, compared with runs of white space collapsed.  Lines
that answer Scarier's name and gender prompts are found by aligning the
golden's "> x" echoes with the solution and are left out, since SCARE never
asks.  See README.md.
"""
import os, re, subprocess, sys
from concurrent.futures import ThreadPoolExecutor

T = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HARNESS = T + '/harness/run_v4_walkthroughs.sh'
GOLD = T + '/goldens'
GAMES = T + '/games'
SEEDS_EXTRA = [1, 2, 3, 4, 5, 6]
PROMPTS = (b'Please enter your name:', b'(male or female):')


def rows():
    data = open(HARNESS, 'rb').read()
    body = data[data.index(b"<<'EOF'"):]
    body = body[body.index(b'\n') + 1:body.index(b'\nEOF\n')]
    for line in body.split(b'\n'):
        if not line.strip() or line.startswith(b'#'):
            continue
        f = line.split(b'|')
        f += [b''] * (4 - len(f))
        sol, game, marker, envs = f[0], f[1], f[2], b'|'.join(f[3:])
        if marker:
            yield sol.decode('latin-1'), game, marker, envs


def prompt_answer_lines(sol_lines, golden):
    """Indices into sol_lines (non-comment lines) that answer Scarier's
    name/gender prompts, found by aligning the golden's echoes."""
    glines = golden.split(b'\n')
    echoes = []  # (text, is_prompt_answer)
    prev = b''
    for g in glines:
        if g == b'>' or g.startswith(b'> '):
            echoes.append((g[2:].strip(), prev.rstrip().endswith(PROMPTS)))
        if g.strip():
            prev = g
    last = max((k for k, (_, a) in enumerate(echoes) if a), default=-1)
    out = []
    i = 0
    for text, is_ans in echoes[:last + 1]:
        j = i
        while j < len(sol_lines) and sol_lines[j].strip() != text:
            j += 1
        if j == len(sol_lines):
            break
        if is_ans:
            out.append(j)
        i = j + 1
    return set(out)


def norm(b):
    return re.sub(rb'\s+', b' ', b)


def run_one(scare, row):
    sol, game, marker, envs = row
    gp = os.path.join(GAMES.encode(), game)
    if not os.path.exists(gp):
        return sol, 'SKIP', 0, ''
    raw = open(os.path.join(GOLD, sol), 'rb').read().split(b'\n')
    lines = [l.rstrip(b'\r') for l in raw if not l.lstrip(b' \t').startswith(b'#')]
    golden = open(os.path.join(GOLD, sol[:-4] + '.expected.txt'), 'rb').read()
    drop = prompt_answer_lines(lines, golden)
    feed = b'\n'.join(l for k, l in enumerate(lines) if k not in drop) + b'\nquit\ny\n'
    env = dict(os.environ)
    rowseed = None
    for kv in envs.split():
        k, _, v = kv.partition(b'=')
        if k == b'SCR_SKIP_WAITKEY':
            env['SCR_SKIP_WAITKEY'] = '1'
        # The harness hands the whole field to env(1), so a row written
        # SCR_SEED=149|SCR_SKIP_WAITKEY=1 sets one variable to "149|..." and
        # no skip; Scarier then reads the leading digits.  Do the same.
        if k == b'SCR_SEED':
            rowseed = int(re.match(rb'\d*', v).group() or b'0')
    seeds = ([rowseed] if rowseed is not None else []) + [s for s in SEEDS_EXTRA if s != rowseed]
    seeds = seeds[:5]
    want = norm(marker).strip()
    statuses = []
    for s in seeds:
        env['SCR_SEED'] = str(s)
        try:
            p = subprocess.run([scare, gp], input=feed, capture_output=True,
                               env=env, timeout=120)
            out, err, rc = p.stdout, p.stderr, p.returncode
        except subprocess.TimeoutExpired as e:
            out, err, rc = e.stdout or b'', b'timeout', -99
        if want in norm(out):
            return sol, 'WIN', s, ''
        e = err.decode('latin-1').strip().replace('\n', ' / ')[:200]
        if b'Not a loadable' in err:
            statuses.append(('NOLOAD', e))
        elif rc != 0 or b'error' in err.lower() or b'assert' in err.lower():
            statuses.append(('ABORT', e or 'rc=%d' % rc))
        else:
            statuses.append(('NOWIN', ''))
    for kind in ('NOLOAD', 'ABORT', 'NOWIN'):
        for st, e in statuses:
            if st == kind:
                return sol, kind, 0, e
    return sol, 'NOWIN', 0, ''


def main():
    scare, out_tsv = sys.argv[1], sys.argv[2]
    flt = sys.argv[3] if len(sys.argv) > 3 else ''
    rs = [r for r in rows() if flt in r[0]]
    with ThreadPoolExecutor(os.cpu_count()) as ex:
        res = list(ex.map(lambda r: run_one(scare, r), rs))
    with open(out_tsv, 'w') as f:
        for (sol, game, _, _), (_, st, seed, e) in zip(rs, res):
            f.write('%s\t%s\t%s\t%s\t%s\n' % (sol, game.decode('latin-1'), st, seed, e))
    from collections import Counter
    print(len(rs), Counter(r[1] for r in res))


main()
