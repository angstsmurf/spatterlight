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
    # order[i] is the same span's markers IN THE ORDER THEY WERE PRINTED, as
    # ("key", None) / ("wait", seconds).  Counting the two kinds separately is
    # not enough: Wheel105's `z` prints its [WAITKEY] BEFORE its 22 seconds of
    # <wait>, so emitting the sleep first left the Runner parked on the pause
    # through the sleep, answered it late, and then typed the next six
    # commands into the waits, which dropped every one of them (2026-09-05).
    pauses = [0]
    waits = [0]
    order = [[]]
    for line in text.split("\n"):
        if line.startswith(">"):
            pauses.append(0)
            waits.append(0)
            order.append([])
            # and fall through: a pause printed by the very first line of a
            # turn's output lands on the prompt line itself (Vardock Bates'
            # newspaper, ">puedes leer en uno de ellos...[WAITKEY]"); losing
            # it left the third of three pauses un-Returned and the Runner's
            # keypress ate the start of the next command (2026-08-29).
        if True:
            # The markers land wherever the interpreter's output cursor is,
            # often mid-line after unterminated text, so search rather than
            # anchor.  A tag's Val() can be fractional; round up.
            for marker in re.finditer(r"\[WAITKEY\]|\[WAIT ([^\]]*)\]", line):
                if marker.group(1) is None:
                    pauses[-1] += 1
                    order[-1].append(("key", None))
                else:
                    try:
                        seconds = int(math.ceil(float(marker.group(1))))
                    except ValueError:
                        seconds = 1
                    waits[-1] += seconds
                    order[-1].append(("wait", seconds))
    with open(solpath, encoding="latin-1") as fh:
        raw = [l.rstrip("\r\n") for l in fh]
    cmds = [l for l in raw if l.strip() and not l.lstrip().startswith("#")]

    # The two BUILT-IN questions.  Scarier asks them inline and reads the
    # answers off stdin like any other command, so the walkthrough's first
    # line or two are the answers -- but the Runner asks them in InputBox
    # dialogs, AT LOAD, before the transcript exists.  Left in the command
    # file they are typed at the game prompt instead: Undefined1.taf's name
    # answer `Undef` came back "That's not going to help." and read as an
    # engine divergence until the feed was re-cut (2026-09-05).  They belong
    # in measure.sh's POPUP_ANSWERS, so say so and leave them out of the file.
    spans = [text.split("\n>")[0]] + text.split("\n>")[1:]
    popups = 0
    for span in spans:
        if ("Please enter your name" in span
                or "Please choose the player's gender" in span):
            popups += 1
            continue
        break
    popup_answers = cmds[:popups]
    cmds = cmds[popups:]
    if popups:
        print('POPUP_ANSWERS="%s"  (%d built-in question(s); these are NOT in'
              " the command file)" % ("|".join(popup_answers), popups))
    if not skip:
        # the solution's own blank lines already stand in for the pauses
        lines = [l for l in raw if not l.lstrip().startswith("#")]
        dropped = 0
        while dropped < popups:
            for i, l in enumerate(lines):
                if l.strip():
                    lines.pop(i)
                    break
            dropped += 1
        # Only the blanks that answer a REAL startup pause become PRE.  A
        # solution may open with blank lines that are ordinary empty commands
        # -- Insane.taf's padded cell answers three of them with "Ha, that's
        # a good one." -- and moving those to PRE sends them before Start
        # Transcript, so three turns vanish from the measurement (2026-09-05).
        # pauses[0] is what SCR_MARK_WAITKEY actually counted before the
        # first prompt; never strip more than that.
        pre = 0
        while lines and not lines[0].strip() and pre < pauses[0]:
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
        # The span AFTER the last prompt has no blank of its own -- the
        # solution simply ends -- but the Runner still stops on every pause in
        # it, and the transcript then breaks off mid-ending.  lostsouls' win
        # text is three <waitkey><cls> beats long and the first drive recorded
        # `> open door` and nothing after it (2026-09-05).
        # order[-1] rather than order[prompt]: without SKIP the marker run's
        # pauses eat lines, so its prompt count and the file's line count are
        # not the same index space.  The last span is the tail either way.
        for kind, seconds in order[-1]:
            outl.append("" if kind == "key" else "#sleep %d" % (seconds + 1))
        with open(out, "w") as fh:
            fh.write("\n".join(outl) + "\n")
        print("PRE=%d (solution not SKIP-wired; its own blank lines kept)"
              "  intro-wait=%ds  sleeps=%d" % (pre, waits[0], sum(waits[1:])))
        return
    pre = pauses[0]
    # Index `order` by PROMPT, not by command.  Under SKIP a blank line in the
    # solution is an empty command: Scarier prompts for it, so it advances the
    # prompt counter -- but it is not sent to the Runner (the compare tool
    # re-aligns the offset that leaves).  Indexing by the command's position
    # instead silently lost every marker after the first blank line.
    # Existence.taf is the case: its solution opens with one blank, so the
    # <waitkey> in its ENDING landed in order[5] while the loop only reached
    # order[4], no Return was emitted for it, and the Runner's transcript broke
    # off mid-ending at "[Press a key when you're ready to continue.]"
    # (2026-09-05).
    numbered = []
    prompt = 0
    seen = 0
    for line in raw:
        if line.lstrip().startswith("#"):
            continue
        prompt += 1
        if not line.strip():
            continue
        seen += 1
        if seen <= popups:
            continue
        numbered.append((prompt, line))
    lines = []
    for at, cmd in numbered:
        lines.append(cmd)
        # everything the NEXT span prints, interleaved as it was printed: a
        # blank Return answers a pause, a #sleep waits out a real-time <wait>.
        for kind, seconds in (order[at] if at < len(order) else []):
            lines.append("" if kind == "key" else "#sleep %d" % (seconds + 1))
    with open(out, "w") as fh:
        fh.write("\n".join(lines) + "\n")
    last = numbered[-1][0] if numbered else 0
    tail = pauses[last] if last < len(pauses) else 0
    print("PRE=%d  commands=%d  mid-game pauses=%d  after-last=%d  intro-wait=%ds  sleeps=%d"
          % (pre, len(cmds), sum(pauses[1:len(cmds)]), tail, waits[0], sum(waits[1:])))


if __name__ == "__main__":
    main()
