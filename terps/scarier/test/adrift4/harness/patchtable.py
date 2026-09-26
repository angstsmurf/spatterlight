#!/usr/bin/env python3
"""Read sctafpar.cpp's built-in per-game patch table.

The table is the engine's own source of truth, so this module parses it rather
than keeping a second copy: every entry here is exactly what
parse_apply_game_patches() applies at load time.

    python3 harness/patchtable.py              # list the table
    python3 harness/patchtable.py <name>       # one entry, as JSON
"""
from __future__ import annotations

import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SCTAFPAR = os.path.normpath(os.path.join(HERE, "..", "..", "..", "sctafpar.cpp"))

# PATCH_SET/ADD/ADD_STRING/STRING/VERIFY, as sctafpar.cpp defines them.
MODES = {"SET": "I", "ADD": "A", "ADD_STRING": "N", "STRING": "S", "VERIFY": "V"}


def _strings(text):
    """Every C string literal in `text`, adjacent ones concatenated."""
    out, buf, i, n = [], None, 0, len(text)
    while i < n:
        ch = text[i]
        if ch == '"':
            i += 1
            lit = []
            while text[i] != '"':
                if text[i] == "\\":
                    lit.append({"n": "\n", "t": "\t", "r": "\r"}.get(text[i + 1],
                                                                     text[i + 1]))
                    i += 2
                else:
                    lit.append(text[i])
                    i += 1
            i += 1
            buf = "".join(lit) if buf is None else buf + "".join(lit)
            continue
        if ch.isspace():
            i += 1
            continue
        if buf is not None:
            out.append(buf)
            buf = None
        i += 1
    if buf is not None:
        out.append(buf)
    return out


def _split_args(text):
    """Top-level comma split of a macro argument list."""
    args, depth, start, instr = [], 0, 0, False
    for i, ch in enumerate(text):
        if instr:
            if ch == "\\":
                instr = "skip"
            elif ch == "skip":
                pass
            elif ch == '"':
                instr = False
            continue
        if ch == '"':
            instr = True
        elif ch in "([":
            depth += 1
        elif ch in ")]":
            depth -= 1
        elif ch == "," and depth == 0:
            args.append(text[start:i])
            start = i + 1
    args.append(text[start:])
    return args


def _edits(body):
    out = []
    for m in re.finditer(r"PATCH_(SET|ADD_STRING|ADD|STRING|VERIFY)\s*\(", body):
        depth, i = 1, m.end()
        instr = False
        while depth:
            ch = body[i]
            if instr:
                if ch == "\\":
                    i += 1
                elif ch == '"':
                    instr = False
            elif ch == '"':
                instr = True
            elif ch == "(":
                depth += 1
            elif ch == ")":
                depth -= 1
            i += 1
        args = _split_args(body[m.end():i - 1])
        kind = m.group(1)
        path = _strings(args[0])[0]
        edit = {"mode": MODES[kind], "kind": kind, "path": path,
                "from_integer": 0, "to_integer": 0,
                "from_string": None, "to_string": None}
        if kind == "SET":
            edit["from_integer"] = int(args[1])
            edit["to_integer"] = int(args[2])
        elif kind == "ADD":
            edit["to_integer"] = int(args[1])
        elif kind == "ADD_STRING":
            edit["to_string"] = _strings(args[1])[0]
        elif kind == "STRING":
            edit["from_string"] = _strings(args[1])[0]
            edit["to_string"] = _strings(args[2])[0]
        elif kind == "VERIFY":
            edit["from_string"] = _strings(args[1])[0]
        out.append(edit)
    return out


def table(path=SCTAFPAR):
    src = open(path, encoding="latin-1").read()
    arrays = {n: _edits(b) for n, b in
              re.findall(r"static const scr_patch_edit_t (PATCH_\w+)\[\] = \{(.*?)\n\};",
                         src, re.S)}
    m = re.search(r"static const scr_patch_game_t PATCH_TABLE\[\] = \{(.*?)\n\};", src, re.S)
    games = []
    for row in re.finditer(r"PATCH_GAME\s*\((.*?)\n?\s*(PATCH_\w+)\)", m.group(1), re.S):
        args = _split_args(row.group(1))
        games.append({"name": _strings(args[0])[0],
                      "author": _strings(args[1])[0],
                      "summary": _strings(args[2])[0],
                      "array": row.group(2),
                      "edits": arrays[row.group(2)]})
    return games


def find(name, author):
    for game in table():
        if game["name"] == name and game["author"] == author:
            return game
    return None


if __name__ == "__main__":
    games = table()
    if len(sys.argv) > 1:
        wanted = sys.argv[1].upper()
        for game in games:
            if game["array"] == wanted or game["array"] == "PATCH_" + wanted:
                print(json.dumps(game, indent=2))
                sys.exit(0)
        sys.exit("no such entry: " + sys.argv[1])
    for game in games:
        print("%-28s %-34s %s" % (game["array"][len("PATCH_"):], game["name"][:34],
                                  " ".join(sorted({e["kind"] for e in game["edits"]}))))
    print("%d games, %d edits" % (len(games), sum(len(g["edits"]) for g in games)))
