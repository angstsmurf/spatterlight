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

import make_patched_taf

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)


def row_for(solution):
    with open(os.path.join(HERE, "run_v4_walkthroughs.sh"), encoding="latin-1") as fh:
        for line in fh:
            if line.startswith(solution + "_solution.txt|"):
                return line.rstrip("\n").split("|")
    sys.exit("no row for %s" % solution)


def without_hints(raw, taf, env):
    """The solution with every `hint` command, and the lines its own [Y/N]
    question read, taken out.

    A row sets WINE_FEED_NO_HINTS=1 for this.  `hint` is a SCARE meta-command
    that asks "Do you really want to view hints? [Y/N]" inline and reads the
    answer off stdin, while run400 answers both lines "I don't understand what
    you mean!", so every pair puts the two sides a turn apart and the compare
    has to re-synchronise on generic text (mould, 2026-09-21).  Neither side's
    game moves on a hint, so the route plays the same without them.

    Which lines those are is read from the replay, not guessed from the text:
    every stdin read is one of the INPUT trace lines (a prompt, which skips
    comments), a "[WAITKEY ate ...]" (a pause, which skips them too) or a
    "[CONFIRM ate ...]" (the question), in the order the lines were read.
    """
    env = dict(env, SCR_TRACE_ADMIN="1", SCR_MARK_WAITKEY="1", SCR_MARK_CONFIRM="1")
    env.pop("SCR_MARK_WAIT", None)
    stdin = "".join(l + "\n" for l in raw).encode("latin-1")
    trace = subprocess.run([os.path.join(HERE, "scare"), taf], input=stdin,
                           stdout=subprocess.DEVNULL, stderr=subprocess.PIPE,
                           env=env).stderr.decode("latin-1")
    reads = re.findall(r'^(?:INPUT line=\d+ (.*)|\[(WAITKEY|CONFIRM) ate "(.*)"\])$',
                       trace, re.M)
    commands = [i for i, l in enumerate(raw) if not l.lstrip().startswith("#")]
    drop = set()
    kind = None
    for n, (typed, marker, ate) in enumerate(reads):
        text = typed if not marker else ate
        if n >= len(commands) or raw[commands[n]].strip() != text.strip():
            sys.exit("WINE_FEED_NO_HINTS: read %d (%r) is not solution line %d"
                     % (n, text, commands[n] + 1 if n < len(commands) else -1))
        if not marker:
            kind = "hint" if typed.strip().lower() == "hint" else None
            if kind:
                drop.add(commands[n])
        elif marker == "CONFIRM" and kind == "hint":
            drop.add(commands[n])
        else:
            kind = None
    return [l for i, l in enumerate(raw) if i not in drop], len(drop)


def main():
    solution, out = sys.argv[1], sys.argv[2]
    row = row_for(solution)
    env = dict(os.environ)
    # A row's env is ONE field per assignment in most rows but 25 of them
    # (warlord, reluctantvampire, house, hcw, ...) space-join two inside a
    # single field, exactly as the harness's own `env $ENV` word-splits it.
    # Partitioning the whole field left SCR_SEED="33 SCR_SKIP_WAITKEY=1" and,
    # worse, hid the SKIP wiring: the replay then stopped at every <waitkey>
    # and ate the next solution line as the answer, so the generated feed was
    # a desynced run of the game (warlord lost 77 blanks, 2026-09-07).
    assignments = [a for field in row[3:] for a in field.split()]
    # A patched row is driven on a copy of the .taf with the patch baked in,
    # so the Runner plays the same game the replay does: take the engine
    # switch off the replay and hand it the same file (make_patched_taf.py).
    taf, assignments = make_patched_taf.game_for(solution, row[1], assignments)
    for assignment in assignments:
        name, _, value = assignment.partition("=")
        env[name] = value
    env["SCR_MARK_WAITKEY"] = "1"
    env["SCR_MARK_WAIT"] = "1"
    skip = "SCR_SKIP_WAITKEY" in env
    solpath = os.path.join(ROOT, "goldens", solution + "_solution.txt")
    with open(solpath, "rb") as fh:
        solution_bytes = fh.read()
    raw = [l.rstrip("\r\n") for l in solution_bytes.decode("latin-1").split("\n")]
    if raw and raw[-1] == "":
        raw.pop()
    if env.get("WINE_FEED_NO_HINTS"):
        raw, dropped = without_hints(raw, taf, env)
        solution_bytes = "".join(l + "\n" for l in raw).encode("latin-1")
        print("WINE_FEED_NO_HINTS: %d hint line(s) and answer(s) left out" % dropped)
    done = subprocess.run([os.path.join(HERE, "scare"), taf], input=solution_bytes,
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
            # A whitespace-only line is a bare Return to Scarier and to the
            # compare, but the driver TYPES it: mould's " " lines reached the
            # Runner as ">  " / "I don't understand what you mean!" where the
            # golden answered a pause with them (2026-09-19).
            outl.append(l if l.strip() else "")
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
    # A blank line under SKIP is an EMPTY COMMAND, and it has to be fed to the
    # Runner like any other: it is a turn, it ticks events, and a game can hang
    # a task off it.  Dropping it (as this did until 2026-09-07, on the theory
    # that the compare tool would re-align the offset) does not merely shift
    # the transcript -- it plays a different game.  CBN.taf is the case: its
    # solution opens with five empty commands, the first of which fires TASK 38
    # and walks the player out of [The Story So Far...], so with them dropped
    # the Runner spent that task on `open door` instead, never opened the door,
    # and every one of the 35 commands after it ran off-route.
    numbered = []
    prompt = 0
    seen = 0
    for line in raw:
        if line.lstrip().startswith("#"):
            continue
        prompt += 1
        if not line.strip():
            numbered.append((prompt, ""))
            continue
        seen += 1
        if seen <= popups:
            continue
        numbered.append((prompt, line))
    lines = []
    # The span BEFORE the first prompt: its pauses are PRE, but its timed
    # <wait>s were never waited out.  Homeless Harry's StartupText runs 82 s
    # of <waitN>; run400 takes typed input during a timed wait, so the first
    # `s` walked the player out mid-intro, echoed with no "> " prompt, and the
    # compare counted it lost (2026-09-24).
    if waits[0]:
        lines.append("#sleep %d" % (waits[0] + 1))
    for n, (at, cmd) in enumerate(numbered):
        lines.append(cmd)
        # everything the NEXT span prints, interleaved as it was printed: a
        # blank Return answers a pause, a #sleep waits out a real-time <wait>.
        # The last command owns every span after it: a game can print its own
        # "> COMMAND" lines, which split the tail into spans no command
        # reaches.  3monkeys' win text prints "> GIVE FINGER TO DR. WICKETT"
        # and "> GIVE FINGER TO DR. GLEE", each followed by a <waitkey>; only
        # the first of its three pauses got a Return, and the Runner's
        # transcript broke off at the first of those lines (2026-09-19).
        spans = order[at:at + 1] if n + 1 < len(numbered) else order[at:]
        for kind, seconds in [m for span in spans for m in span]:
            lines.append("" if kind == "key" else "#sleep %d" % (seconds + 1))
    with open(out, "w") as fh:
        fh.write("\n".join(lines) + "\n")
    last = numbered[-1][0] if numbered else 0
    tail = sum(pauses[last:])
    print("PRE=%d  commands=%d  mid-game pauses=%d  after-last=%d  intro-wait=%ds  sleeps=%d"
          % (pre, len(cmds), sum(pauses[1:len(cmds)]), tail, waits[0], sum(waits[1:])))


if __name__ == "__main__":
    main()
