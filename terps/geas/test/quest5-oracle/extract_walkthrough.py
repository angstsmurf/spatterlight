#!/usr/bin/env python3
"""Convert a Quest 5 walkthrough into a flat qvh command script.

Two source formats occur in the corpus:

  welbourn  David Welbourn / Key & Compass: commands are ">"-prefixed and packed
            several-per-line with ". " separators; parentheticals are prose.
  commands  A bare one-command-per-line list (often the tail of an author doc,
            after a prose section), possibly indented and using qvh directives
            (menu:/answer:). Prose lines are dropped by a shape heuristic.

Usage: extract_walkthrough.py [--mode welbourn|commands] <file>
Mode defaults to welbourn if the file has any ">"-prefixed line, else commands.
"""
import re, sys


# Welbourn's "either command works" notation, e.g. "1 —or— 2" or
# "ask X about a —or— ask X about b" (em/en dash). Take the first alternative.
_OR = re.compile(r"\s*[—–]\s*or\s*[—–]\s*")


def extract_welbourn(text):
    cmds = []
    for line in text.splitlines():
        if not line.lstrip().startswith(">"):
            continue
        body = line.lstrip()[1:].strip()
        body = re.sub(r"\([^)]*\)", "", body)          # drop "(asides)"
        for part in re.split(r"\.\s+|\.$", body):
            c = _OR.split(part, 1)[0].strip().rstrip(".").strip()
            if c:
                cmds.append(c)
    return cmds


_DIRECTIVE = re.compile(r"^(menu:|answer:|assert:|event:|label:|delay:)")


def _looks_like_command(s):
    if _DIRECTIVE.match(s):
        return True
    if not re.search(r"[a-z]", s):                     # headers: ALLCAPS, ---- , 5)
        return s.isdigit()                             # ...but keep bare menu numbers
    if s.isdigit():
        return True
    if s[0].isupper():                                 # prose/headers start uppercase
        return False
    if len(s.split()) > 6:                             # commands are terse
        return False
    if s[-1] in ".!?:":                                # sentences end in punctuation
        return False
    return True


def extract_commands(text):
    return [s for line in text.splitlines()
            if (s := line.strip()) and _looks_like_command(s)]


def extract(text, mode=None):
    if mode is None:
        mode = "welbourn" if any(l.lstrip().startswith(">") for l in text.splitlines()) \
               else "commands"
    return extract_welbourn(text) if mode == "welbourn" else extract_commands(text)


if __name__ == "__main__":
    args = sys.argv[1:]
    mode = None
    if args and args[0] == "--mode":
        mode = args[1]
        args = args[2:]
    src = open(args[0], encoding="utf-8", errors="replace").read()
    for c in extract(src, mode):
        print(c)
