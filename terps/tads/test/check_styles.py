#!/usr/bin/env python3
"""Check glk/osstyles.c's count of style combinations against a Python one.

Builds test/stylescan.c with cc, runs it on every game in a corpus, and
compares its counts with the ones counted here, which follow
.work/bin/stylecount.py (the script that HTML_TODO.md's numbers come from)
but keep only the five attributes osstyles.c keeps.  Then it says which
games have combinations that get no style of their own.

    check_styles.py [--corpus htmltads|xtads] [--no-escapes] [--table]
                    [substring ...]

  --no-escapes  don't count TADS 2's \\( and \\) as bold, as stylecount.py
                doesn't
  --table       print the style chosen for each game
  substring     only the games whose path contains one of these
"""

import os
import re
import struct
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
GLK = os.path.join(HERE, "..", "glk")
GLKIMP = os.path.join(HERE, "..", "..", "..", "glkimp")
LISTS = {"htmltads": "CENSUS.tsv", "xtads": "games.manifest.tsv"}

T2SIG = b"TADS2 bin\n\r\x1a"
T3SIG = b"T3-image\r\n\x1a"

BOLD, ITALIC, FIXED, INPUT, CENTER = 1, 2, 4, 8, 16
STYLES = ["Normal", "Emphasized", "Preformatted", "Header", "Subheader",
          "Alert", "Note", "BlockQuote", "Input", "User1", "User2"]


def u2(b, o):
    return struct.unpack_from("<H", b, o)[0]


def u4(b, o):
    return struct.unpack_from("<I", b, o)[0]


def xor(data, seed, inc):
    out = bytearray(data)
    for i in range(len(out)):
        out[i] ^= (seed + inc * i) & 0xff
    return bytes(out)


def t2_blobs(b):
    """the OBJ objects and functions and FMTSTR, as tadsscan.py has them"""
    crypt = u2(b, 20) & 0x08
    seed, inc = 17, 29
    pos = 48
    while pos < len(b):
        n = b[pos]
        name = b[pos + 1:pos + 1 + n]
        end = u4(b, pos + 1 + n)
        body = pos + 1 + n + 4
        if name == b"$EOF" or end > len(b) or end < body:
            break
        if name == b"XSI":
            seed, inc = b[body], b[body + 1]
        elif name == b"OBJ":
            p = body
            while p < end:
                t = b[p]
                if t in (1, 2):
                    dsz = u2(b, p + 5)
                    d = b[p + 7:p + 7 + dsz]
                    yield xor(d, seed, inc) if crypt else d
                    p += 7 + dsz
                elif t in (7, 8):
                    p += 5 + u2(b, p + 3)
                elif t == 10:
                    p += 4 + b[p + 3]
                else:
                    break
        elif name == b"FMTSTR":
            ln = min(u2(b, body), end - body - 2)
            d = b[body + 2:body + 2 + ln]
            yield xor(d, seed, inc) if crypt else d
        pos = end


def t3_blobs(b):
    """every CPPG page, XOR'd with its mask"""
    pos = 69
    while pos + 10 <= len(b):
        typ, sz = b[pos:pos + 4], u4(b, pos + 4)
        body = pos + 10
        if typ == b"EOF " or sz > len(b) - body:
            break
        if typ == b"CPPG" and sz >= 7:
            mask = b[body + 6]
            d = b[body + 7:body + sz]
            yield bytes(c ^ mask for c in d) if mask else d
        pos = body + sz


TAG = rb'<\s*(/?)\s*([A-Za-z][A-Za-z0-9]*)((?:[^<>"\']|"[^"]*"|\'[^\']*\')*)>'
TOK = re.compile(TAG + rb'|([^<]+)')
# with \( and \): a text run stops at them, and "\\" is one character
TOK_ESC = re.compile(TAG + rb'|\\([()])'
                     rb'|((?:[^<\\]|\\[^()<]|\\(?=<)|\\\Z)+)')
ATTR = re.compile(rb'([A-Za-z][-A-Za-z0-9]*)'
                  rb'(?:\s*=\s*("[^"]*"|\'[^\']*\'|[^\s>]+))?')
FIX = {"TT", "CODE", "KBD", "SAMP", "PRE", "LISTING", "XMP"}
OTHER = {"S", "STRIKE", "DEL", "BIG", "SMALL", "SUP", "SUB",
         "BLOCKQUOTE", "BQ", "UL", "OL", "DL"}


def family(face):
    f = face.lower()
    if "input" in f:
        return "input"
    if any(k in f for k in ("typewriter", "courier", "mono", "fixed",
                            "console")):
        return "mono"
    return "other"


def count(blobs, escapes):
    counts = {}
    tok = TOK_ESC if escapes else TOK
    for s in blobs:
        st = []                         # (name, changes)
        for m in tok.finditer(s):
            text = m.group(5) if escapes else m.group(4)
            if text is not None:
                if not text.strip() or not st:
                    continue
                k = {"b": 0, "i": 0, "fam": "", "align": 0}
                for _, ch in st:
                    k.update(ch)
                c = ((BOLD if k["b"] else 0) | (ITALIC if k["i"] else 0)
                     | (CENTER if k["align"] else 0)
                     | (FIXED if k["fam"] == "mono" else 0)
                     | (INPUT if k["fam"] == "input" else 0))
                counts[c] = counts.get(c, 0) + 1
                continue
            if escapes and m.group(4) is not None:
                if m.group(4) == b"(":
                    st.append(("\\(", {"b": 1}))
                else:
                    pop(st, "\\(")
                continue
            close, n = m.group(1), m.group(2).decode("latin1").upper()
            if close:
                pop(st, n)
                continue
            at = {a.group(1).decode("latin1").upper():
                  (a.group(2) or b"").strip(b"\"'").decode("latin1")
                  for a in ATTR.finditer(m.group(3))}
            centred = at.get("ALIGN", "").lower() in ("center", "right")
            ch = None
            if n in ("B", "STRONG"):
                ch = {"b": 1}
            elif n in ("I", "EM", "CITE", "VAR", "DFN", "ADDRESS", "U"):
                ch = {"i": 1}
            elif n in FIX:
                ch = {"fam": "mono"}
            elif n in OTHER:
                ch = {}
            elif n == "FONT":
                ch = {"fam": family(at["FACE"])} if "FACE" in at else {}
            elif re.fullmatch(r"H[1-6]", n):
                ch = {"b": 1, "align": 1} if centred else {"b": 1}
            elif n == "CENTER" or (n in ("P", "DIV") and centred):
                ch = {"align": 1}
            if ch is not None:
                st.append((n, ch))
    return counts


def pop(st, name):
    for j in range(len(st) - 1, -1, -1):
        if st[j][0] == name:
            del st[j:]
            return


def py_count(path, escapes):
    b = open(path, "rb").read()
    if b.startswith(T2SIG):
        return count(t2_blobs(b), escapes)
    if b.startswith(T3SIG):
        return count(t3_blobs(b), False)
    return None


def build(tmp):
    exe = os.path.join(tmp, "stylescan")
    subprocess.run(["cc", "-O2", "-Wall", "-I", GLK, "-I", GLKIMP, "-o", exe,
                    os.path.join(HERE, "stylescan.c"),
                    os.path.join(GLK, "osstyles.c")], check=True)
    return exe


def games(corpus, filters):
    folder = os.path.join(HERE, corpus)
    out = []
    for line in open(os.path.join(folder, LISTS[corpus]), encoding="utf-8"):
        if line.startswith("#") or not line.strip():
            continue
        path = line.split("\t")[0]
        if not path.lower().endswith((".gam", ".t3")):
            continue
        if filters and not any(f in path for f in filters):
            continue
        full = os.path.join(folder, "games", path)
        if os.path.exists(full):
            out.append(full)
    return out


def combo_name(c):
    if c == 0:
        return "plain"
    names = [n for bit, n in ((CENTER, "centred"), (BOLD, "bold"),
                              (ITALIC, "italic"), (FIXED, "fixed"),
                              (INPUT, "Input")) if c & bit]
    return " ".join(names)


def main():
    args = sys.argv[1:]
    corpus, escapes, table = "htmltads", True, False
    filters = []
    while args:
        a = args.pop(0)
        if a == "--corpus":
            corpus = args.pop(0)
        elif a == "--no-escapes":
            escapes = False
        elif a == "--table":
            table = True
        else:
            filters.append(a)

    paths = games(corpus, filters)
    with tempfile.TemporaryDirectory() as tmp:
        exe = build(tmp)
        out = subprocess.run([exe] + ([] if escapes else ["-n"]) + paths,
                             capture_output=True, check=True).stdout
    bad = 0
    lost = []
    for line in out.decode("utf-8", "replace").splitlines():
        path, counts, chosen = line.split("\t")
        name = os.path.relpath(path, os.path.join(HERE, corpus, "games"))
        mine = None if counts == "-" else {
            int(c, 16): int(n) for c, n in
            (x.split(":") for x in counts.split())}
        ref = py_count(path, escapes)
        if mine != ref:
            bad += 1
            print("DIFFERS %s\n  C:      %s\n  Python: %s"
                  % (name, sorted((mine or {}).items()),
                     sorted((ref or {}).items())))
            continue
        styles = {int(c, 16) for s, c in
                  (x.split("=") for x in chosen.split())} if mine else set()
        without = sorted(c for c in (mine or {}) if c not in styles)
        if without:
            lost.append((name, without))
        if table:
            print("%s: %s" % (name, ", ".join(
                "%s=%s" % (STYLES[int(s)], combo_name(int(c, 16)))
                for s, c in (x.split("=") for x in chosen.split()))))
    for name, without in lost:
        print("no style of its own in %s: %s"
              % (name, ", ".join(combo_name(c) for c in without)))
    print("%d games, %d differ, %d with a combination that has no style"
          % (len(paths), bad, len(lost)))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
