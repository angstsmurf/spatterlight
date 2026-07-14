#!/usr/bin/env python3
"""Convert a David Welbourn (Key & Compass) walkthrough .txt into a flat
one-command-per-line script for the qvh oracle.

Welbourn format: command lines begin with "> " and pack several commands
separated by ". ". Parenthetical asides are prose, not commands.
"""
import re, sys

def extract(text):
    cmds = []
    for line in text.splitlines():
        line = line.rstrip()
        if not line.startswith(">"):
            continue
        body = line[1:].strip()
        # drop parenthetical asides: "(too heavy!)", "(gives you Port.)"
        body = re.sub(r"\([^)]*\)", "", body)
        # split into individual commands on period boundaries
        for part in re.split(r"\.\s+|\.$", body):
            c = part.strip().rstrip(".").strip()
            if c:
                cmds.append(c)
    return cmds

if __name__ == "__main__":
    src = open(sys.argv[1], encoding="utf-8", errors="replace").read()
    for c in extract(src):
        print(c)
