#!/usr/bin/env python3
"""Screen a wired v4 game before driving it through the Wine Runner.

Everything here answers one of the pre-flight questions in
notes/WINE-TRANSCRIPTS-TODO.md ("Before measuring anything"), from the
SCR_DUMP_TASKS dump rather than by eye:

  version   the .taf's engine version, so the right Runner is picked
  cmds      REAL commands in the wired solution (the candidate table counts
            golden LINES, comments included, and is routinely 2x too high)
  events    how many events exist, and how many of them can actually ROLL --
            event lengths, StarterType=2 start delays and restart re-rolls
            all draw lo + Int(Rnd*(hi-lo)), exclusive of the upper bound, so
            a lo..hi only rolls when hi - lo >= 2.  A rollable event whose
            texts can reach the route is the one thing that makes a row
            unmeasurable.
  npcs      characters, which is the other source of per-turn output
  silent    tasks with NO CompleteText.  Pre-4.0 Runners let such a task
            claim the command and then print the game's DontUnderstand
            string instead of falling through to the library; Scarier falls
            through.  A silent task whose pattern the walkthrough actually
            TYPES is a guaranteed (deliberate) divergence -- see the Hangover
            filing cabinet and everything.taf's `read diary`.  Patterns
            beginning with `!` or `#` are task names, not typeable commands.

Usage:
    python3 screen_wine_candidate.py <game.taf> [<game.taf> ...]

The argument is the .taf's bare name exactly as run_v4_walkthroughs.sh spells
it in the row's second field -- `mhpquest.taf`, not `games/mhpquest.taf` --
because the row is what supplies the golden and the row's env.
"""
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

RANGE_RE = re.compile(r"(?:start|time1)=(\d+)(?:\.\.| time2=)(\d+)")


def rows():
    out = {}
    path = os.path.join(HERE, "run_v4_walkthroughs.sh")
    for line in open(path, encoding="latin-1"):
        if line.startswith("#") or "|" not in line:
            continue
        parts = line.rstrip("\n").split("|")
        if parts[0].endswith("_solution.txt"):
            out.setdefault(parts[1], parts)
    return out


def solution(row):
    return os.path.join(ROOT, "goldens", row[0])


def dump(taf, row):
    env = dict(os.environ)
    for assignment in row[3:]:
        name, _, value = assignment.partition("=")
        env[name] = value
    # A startup <waitkey> would eat the single turn the dump needs to be
    # emitted at all, and the dump would come back empty -- which once read
    # as "0 events, 0 NPCs" for two games that have plenty (2026-09-05).
    env["SCR_SKIP_WAITKEY"] = "1"
    env["SCR_DUMP_TASKS"] = "1"
    # The dump is emitted at the end of the first TURN, so the feed has to
    # reach one.  A bare "look" does not for every game: Villains_And_Kings
    # asks for a name and a gender first and swallows it, and the dump comes
    # back empty (2026-09-05).  Replaying the row's own opening lines gets
    # past any such prompt, whatever shape it takes.
    opening = [l.rstrip("\n") for l in open(solution(row), encoding="latin-1")
               if not l.startswith("#")][:8]
    feed = "\n".join(opening + ["look"]) + "\n"
    done = subprocess.run([os.path.join(HERE, "scare"),
                           os.path.join(ROOT, "games", taf)],
                          input=feed.encode("latin-1"),
                          stdout=subprocess.DEVNULL,
                          stderr=subprocess.PIPE, env=env)
    return done.stderr.decode("latin-1").split("\n")


def screen(taf, row):
    lines = dump(taf, row)
    if not any(l.startswith("GAME ") for l in lines):
        return "%-32s EMPTY DUMP (does it load?)" % taf
    version = [l for l in lines if l.startswith("GAME ")][0].split()[1]

    commands = sum(1 for l in open(solution(row), encoding="latin-1")
                   if l.strip() and not l.startswith("#"))

    events = [l for l in lines if l.startswith("EVENT ")]
    rollable = [l for l in events
                if any(int(hi) - int(lo) >= 2
                       for lo, hi in RANGE_RE.findall(l))]
    npcs = [l for l in lines if l.startswith("NPC ")]

    silent = []
    for i, line in enumerate(lines):
        if not line.startswith("TASK "):
            continue
        j, body = i + 1, []
        while j < len(lines) and lines[j].startswith("    "):
            body.append(lines[j])
            j += 1
        if any(b.startswith("    COMPLETE=") for b in body):
            continue
        patterns = [line.split("cmd=")[-1].strip("[]")]
        patterns += [b.split("=", 1)[1].strip()[1:-1]
                     for b in body if b.strip().startswith("ALTCMD")]
        silent.append([p for p in patterns if p[:1] not in ("!", "#")])

    typeable = [p for group in silent for p in group]
    note = ""
    if rollable:
        note += "  ROLLABLE: " + "; ".join(
            l.split("[")[1].split("]")[0] for l in rollable)
    if typeable:
        note += "  SILENT-TYPEABLE: " + "; ".join(typeable[:6])
    return ("%-32s %s cmds=%-4d events=%-3d rollable=%-3d npcs=%-3d "
            "silent=%d%s" % (taf, version, commands, len(events),
                             len(rollable), len(npcs), len(silent), note))


def main():
    table = rows()
    for taf in sys.argv[1:]:
        row = table.get(taf)
        if row is None:
            print("%-32s NO ROW in run_v4_walkthroughs.sh" % taf)
            continue
        print(screen(taf, row))


if __name__ == "__main__":
    main()
