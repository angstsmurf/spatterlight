#!/usr/bin/env python3
"""Turn a wired v4 solution into a Wine Runner command file.

The Runner has no SCR_SKIP_WAITKEY: every <waitkey> pause it reaches eats
the next keystroke.  So a solution that is replayed through
~/adrift-battle/runner/wine/drive_ckpt_safe.sh needs one BLANK line (a bare
Return) in front of every command a pause precedes, and the pauses in the
game's opening -- before the first command -- have to go to measure.sh's
PRE argument instead, because the Adventure menu is dead until they are
dismissed.

This derives both from the harness: SCR_MARK_WAITKEY=1 prints "[WAITKEY]"
on stderr in transcript order, so with 2>&1 the markers fall between the
`>` prompts they belong to.

Usage:
    python3 make_wine_cmdfile.py <solution-basename> <out cmdfile>
prints "PRE=<n>" on stdout; reads the row's env from run_v4_walkthroughs.sh.
"""
import os
import math
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)


def row_for(solution):
    with open(os.path.join(HERE, "run_v4_walkthroughs.sh"), encoding="latin-1") as fh:
        for line in fh:
            if line.startswith(solution + "_solution.txt|"):
                return line.rstrip("\n").split("|")
    sys.exit("no row for %s" % solution)


def main():
    solution, out = sys.argv[1], sys.argv[2]
    row = row_for(solution)
    taf = os.path.join(ROOT, "games", row[1])
    env = dict(os.environ)
    for assignment in row[3:]:
        name, _, value = assignment.partition("=")
        env[name] = value
    env["SCR_MARK_WAITKEY"] = "1"
    env["SCR_MARK_WAIT"] = "1"
    skip = "SCR_SKIP_WAITKEY" in env
    solpath = os.path.join(ROOT, "goldens", solution + "_solution.txt")
    with open(solpath, "rb") as fh:
        done = subprocess.run([os.path.join(HERE, "scare"), taf], stdin=fh,
                              stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                              env=env)
    text = done.stdout.decode("latin-1")
    # pauses[i] = pauses printed after prompt i-1 and before prompt i
    # (pauses[0] = before the first prompt).
    # waits[i] = seconds of real-time <wait N> pauses in the same span; the
    # Runner drops keystrokes typed while one runs, so the feed must sleep.
    pauses = [0]
    waits = [0]
    for line in text.split("\n"):
        if line.startswith(">"):
            pauses.append(0)
            waits.append(0)
            # and fall through: a pause printed by the very first line of a
            # turn's output lands on the prompt line itself (Vardock Bates'
            # newspaper, ">puedes leer en uno de ellos...[WAITKEY]"); losing
            # it left the third of three pauses un-Returned and the Runner's
            # keypress ate the start of the next command (2026-08-29).
        if True:
            # The markers land wherever the interpreter's output cursor is,
            # often mid-line after unterminated text, so search rather than
            # anchor.  A tag's Val() can be fractional; round up.
            pauses[-1] += len(re.findall(r"\[WAITKEY\]", line))
            for arg in re.findall(r"\[WAIT ([^\]]*)\]", line):
                try:
                    waits[-1] += int(math.ceil(float(arg)))
                except ValueError:
                    waits[-1] += 1
    with open(solpath, encoding="latin-1") as fh:
        raw = [l.rstrip("\r\n") for l in fh]
    cmds = [l for l in raw if l.strip() and not l.lstrip().startswith("#")]
    if not skip:
        # the solution's own blank lines already stand in for the pauses
        lines = [l for l in raw if not l.lstrip().startswith("#")]
        pre = 0
        while lines and not lines[0].strip():
            lines.pop(0); pre += 1
        # the harness eats a line per pause, so command i is prompt i only
        # while no pause has eaten a blank; sleeps are keyed by prompt index
        outl = []
        prompt = 0
        for l in lines:
            outl.append(l)
            prompt += 1
            if prompt < len(waits) and waits[prompt]:
                outl.append("#sleep %d" % (waits[prompt] + 1))
        with open(out, "w") as fh:
            fh.write("\n".join(outl) + "\n")
        print("PRE=%d (solution not SKIP-wired; its own blank lines kept)"
              "  intro-wait=%ds  sleeps=%d" % (pre, waits[0], sum(waits[1:])))
        return
    pre = pauses[0]
    lines = []
    for i, cmd in enumerate(cmds):
        if i > 0 and i < len(pauses):
            lines.extend([""] * pauses[i])
        lines.append(cmd)
        if i + 1 < len(waits) and waits[i + 1]:
            lines.append("#sleep %d" % (waits[i + 1] + 1))
    with open(out, "w") as fh:
        fh.write("\n".join(lines) + "\n")
    tail = pauses[len(cmds)] if len(pauses) > len(cmds) else 0
    print("PRE=%d  commands=%d  mid-game pauses=%d  after-last=%d  intro-wait=%ds  sleeps=%d"
          % (pre, len(cmds), sum(pauses[1:len(cmds)]), tail, waits[0], sum(waits[1:])))


if __name__ == "__main__":
    main()
