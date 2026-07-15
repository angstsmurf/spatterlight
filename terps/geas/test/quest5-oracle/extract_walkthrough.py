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

# Inline menu / get-input answer. Welbourn writes an unprefixed line quoting the
# game's prompt and ending in the number he chose, e.g. "...thwarted
# expectations? 1" (a numbered text menu) or "What combination do you enter?
# 987333" (a get-input answer). A real ">" command never contains "?", so the
# trailing digits are unambiguously an answer to send as its own turn — missing
# it leaves a text menu pending, which silently swallows every later command.
_MENU_ANSWER = re.compile(r"\?\s+(\d+)\s*$")

# Welbourn's keypress notation for a "press any key" / timed prompt (only Escape
# From the Mechanical Bathhouse uses it). The harness auto-continues wait/DoPause
# prompts without consuming a script line, so a literal SPACE would just be a
# bogus command; drop it. "SPACESPACE" is two presses collapsed by the "."-split.
_KEYPRESS = re.compile(r"^(?:SPACE)+$")

# Welbourn sprinkles inline dingbat/emoji annotations into command lines — ☹/☺
# to flag a note, ★ for a milestone — e.g. "> x figurehead. take it. ☹ w." Left
# in, "☹ w" is an unrecognised command, so the move silently fails and every
# later command (examining objects in rooms never reached) cascades into "I
# can't see that". Strip the symbol/dingbat/emoji ranges; the em/en dashes used
# by the "—or—" notation sit outside them and are preserved for _OR.
_GLYPH = re.compile("[☀-➿⬀-⯿️\U0001f000-\U0001faff]")


def extract_welbourn(text):
    cmds = []
    for line in text.splitlines():
        if not line.lstrip().startswith(">"):
            m = _MENU_ANSWER.search(line)
            if m:
                cmds.append(m.group(1))
            continue
        body = line.lstrip()[1:].strip()
        body = re.sub(r"\([^)]*\)", "", body)          # drop "(asides)"
        body = _GLYPH.sub("", body)                    # drop ☹/☺/★ annotations
        for part in re.split(r"\.\s+|\.$", body):
            c = _OR.split(part, 1)[0].strip().rstrip(".").strip()
            if c and not _KEYPRESS.match(c):
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
