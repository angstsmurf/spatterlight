#!/usr/bin/env python3
"""Grep task-command patterns across a corpus of ADRIFT 3.8/3.9/4.0 .taf files.

Measuring *corpus exposure* is what turns a parser divergence into a decision:
a shape no game authors is documented, a shape ten games author gets fixed.
`SCR_DUMP_TASKS` (which harness/build.sh does compile in) dumps one loaded
game's structure; measuring a *pattern* across a whole corpus wants the task
command text of every game at once, so this reaches the same data the cheap
way -- de-obfuscate the file and regex the plain text.

  * v4.0  = 14-byte signature + 8 ASCII digits + zlib stream (+ 15-byte
            trailer, see taftool.py next to this script).
  * v3.9 / v3.8 = 14-byte signature, then the body xor'd with the Visual
            Basic PRNG indexed by ABSOLUTE FILE OFFSET.
  * ADRIFT 5 files share the first eight signature bytes; they decode to
            noise here and are reported as skipped.

Usage:
    python3 taf_pattern_scan.py 'games/*.taf'                # default: a `*`
                                                             # or `%text%`
                                                             # inside a group
    python3 taf_pattern_scan.py 'games/*.taf' '%character%'  # any regex
"""
import sys, os, re, zlib, glob

SIG40 = bytes.fromhex('3c423fc96a87c2cf93453e6139fa')


def _prng(n):
    """First n values of the VB6 PRNG (Rnd(-1); Randomize 1976)."""
    s = 0x00a09e86
    out = bytearray()
    for _ in range(n):
        s = (s * 0x43fd43fd + 0x00c39ec3) & 0x00ffffff
        out.append((255 * s) // 0x1000000)
    return out


def plaintext(path):
    """-> (body bytes, version label).  Raises on anything unrecognisable."""
    d = open(path, 'rb').read()
    if d[:14] == SIG40:
        return zlib.decompressobj().decompress(d[22:]), '4.0'
    r = _prng(len(d))
    return bytes(d[14 + i] ^ r[14 + i] for i in range(len(d) - 14)), '3.9/3.8'


# A bracket/brace group with no nesting -- ADRIFT's command syntax has none.
GROUP = re.compile(r'\[[^\[\]{}]*\]|\{[^\[\]{}]*\}')


def group_tokens(line):
    """The default scan: `*` or `%text%` sitting INSIDE a group.

    run400 refuses such a pattern for every input (RUNNER_TESTS_TODO.md §4);
    Scarier matches two cells of it, so this measures the exposure.
    """
    out = []
    for m in GROUP.finditer(line):
        inner = m.group(0)[1:-1]
        if '*' in inner or '%text%' in inner:
            at_end = m.end() >= len(line.rstrip())
            out.append('%s:%s' % (m.group(0), 'trailing' if at_end else 'mid'))
    return out


def main():
    pattern = sys.argv[2] if len(sys.argv) > 2 else None
    rx = re.compile(pattern) if pattern else None
    hits = skipped = 0
    for path in sorted(glob.glob(sys.argv[1])):
        name = os.path.basename(path)
        try:
            body, ver = plaintext(path)
            text = body.decode('latin-1')
        except Exception as exc:
            print('SKIP %-45s %s' % (name, str(exc)[:60]))
            skipped += 1
            continue
        # A body that decodes to mostly non-text was never a 3.8/3.9 file
        # (ADRIFT 5 .taf land here).
        printable = sum(1 for c in body if 32 <= c < 127 or c in (9, 10, 13))
        if body and printable / len(body) < 0.9:
            print('SKIP %-45s not a v4-era file (binary after de-obfuscation)'
                  % name)
            skipped += 1
            continue
        for line in text.split('\r\n'):
            if len(line) > 300:
                continue
            if rx:
                if not rx.search(line):
                    continue
                why = ''
            else:
                found = group_tokens(line)
                if not found:
                    continue
                why = '  ' + ' '.join(found)
            hits += 1
            print('%-45s %-8s %r%s' % (name, ver, line, why))
    print('--- %d matching lines, %d files skipped' % (hits, skipped))


if __name__ == '__main__':
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    main()
