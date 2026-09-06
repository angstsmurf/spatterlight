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
  silent    tasks that print NOTHING when they run: no CompleteText, no
            ShowRoomDesc, and no End game action.  A task that shows a room
            description prints (impulso.taf's `atacar * chico`, srd=5), and
            so does one that ends the game -- the game's own win or lose text
            (Dreams.taf's `pour * water * ... into * basin`, and
            Toxically_Earth before it).  Both measured CLEAN.  Pre-4.0 Runners let such a task
            claim the command and then print the game's DontUnderstand
            string instead of falling through to the library; Scarier falls
            through.  A silent task whose pattern the walkthrough actually
            TYPES is a guaranteed (deliberate) divergence -- see the Hangover
            filing cabinet and everything.taf's `read diary`.  Patterns
            beginning with `!` or `#` are task names, not typeable commands.
            A REVERSIBLE task counts twice: its ReverseCommand patterns are
            typeable as well, and when its ReverseMessage is empty the reverse
            run is the silent one even though the forward run has a
            CompleteText.  lifesimulation's task 10 `turn on tv` reverses on
            the literal `turn off tv` and says nothing; that cost five probe
            drives to find because the dump showed neither the flag nor the
            reverse commands (2026-09-05).

The TYPED note is the one that matters: a silent pattern the solution never
types cannot diverge.  The converse is not a verdict, only a candidate: what
the rule turns on is whether the TURN printed anything, and a task can print
from its actions as well as from its CompleteText.  Wildcards are matched loosely (`*` is any run of
words), so `* channel 1 *` counts as typed by `channel 1`.

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
    # The dump is emitted at the end of the first TURN, so the feed has to
    # reach one.  A bare "look" does not for every game: Villains_And_Kings
    # asks for a name and a gender first and swallows it, and the dump comes
    # back empty (2026-09-05).  Replaying the row's own opening lines gets
    # past any such prompt, whatever shape it takes.
    opening = [l.rstrip("\n") for l in open(solution(row), encoding="latin-1")
               if not l.startswith("#")][:16]
    feed = ("\n".join(opening + ["look"]) + "\n").encode("latin-1")

    def attempt(skip):
        env = dict(os.environ)
        for assignment in row[3:]:
            name, _, value = assignment.partition("=")
            env[name] = value
        if skip:
            env["SCR_SKIP_WAITKEY"] = "1"
        env["SCR_DUMP_TASKS"] = "1"
        done = subprocess.run([os.path.join(HERE, "scare"),
                               os.path.join(ROOT, "games", taf)],
                              input=feed, stdout=subprocess.DEVNULL,
                              stderr=subprocess.PIPE, env=env)
        return done.stderr.decode("latin-1").split("\n")

    # Run the opening the way the ROW runs it first.  Forcing
    # SCR_SKIP_WAITKEY on every game looks like the safe choice -- a startup
    # <waitkey> otherwise eats the single turn the dump needs -- but it is not:
    # a solution that is not SKIP-wired answers those pauses with blank lines
    # of its own, and skipping the pauses turns each of those blanks into an
    # empty command.  Phoenix_Destiny.taf opens with twelve pauses, and with
    # them skipped the twelve blanks walk it straight off the end of its
    # prologue and the process exits before any turn at all -- which is why it
    # read as EMPTY DUMP for a week (2026-09-05).  Only if the row's own way
    # reaches no turn is the skip worth trying.
    lines = attempt("SCR_SKIP_WAITKEY" in " ".join(row[3:]))
    if not any(l.startswith("GAME ") for l in lines):
        lines = attempt(True)
    return lines


def types_pattern(row, pattern):
    """True if the wired solution types a command this pattern would match.

    Cheap stand-in for the real matcher: `*` and `%text%`/`%object%` stand for
    anything at all (including nothing -- everything.taf's `read * diary *` is
    typed as a bare `read diary`), everything else has to appear literally.
    It only has to be good enough to say "look at this one before driving it".
    """
    rx, previous = r"^\s*", None
    for token in pattern.split():
        if token == "*" or (token.startswith("%") and token.endswith("%")):
            rx, previous = rx + ".*", "wild"
            continue
        if previous == "literal":
            rx += r"\s+"
        rx, previous = rx + re.escape(token), "literal"
    matcher = re.compile(rx + r"\s*$", re.I)
    for line in open(solution(row), encoding="latin-1"):
        if line.startswith("#"):
            continue
        if matcher.match(line.rstrip("\n")):
            return True
    return False


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
        # A task that shows a room description prints even with no
        # CompleteText, so its turn is not silent.  impulso.taf's task 8
        # `atacar * chico` has no CompleteText and srd=5, and the row measured
        # CLEAN against run390 -- without this the screener called it a
        # guaranteed divergence (2026-09-05).
        shows_room = " srd=0 " not in line
        # An End game action (ACT type=6) prints the game's own win or lose
        # text, so that turn is not silent either -- Dreams.taf's task 1
        # `pour * water * from * waterskin into * basin` has no CompleteText,
        # ends the game, and measured CLEAN, exactly like Toxically_Earth
        # (2026-09-05).
        ends_game = any(b.startswith("    ACT type=6 ") for b in body)
        # forward half: no CompleteText at all
        if not shows_room and not ends_game \
                and not any(b.startswith("    COMPLETE=") for b in body):
            patterns = [line.split("cmd=")[-1].strip("[]")]
            patterns += [b.split("=", 1)[1].strip()[1:-1]
                         for b in body if b.strip().startswith("ALTCMD")]
            silent.append([p for p in patterns
                           if p.strip() and p[:1] not in ("!", "#")])
        # reverse half: reversible with an empty ReverseMessage
        if (any(b.startswith("    REVERSIBLE ") for b in body)
                and not any(b.startswith("    REVERSE=") for b in body)):
            patterns = [b.split("=", 1)[1].strip()[1:-1]
                        for b in body if b.strip().startswith("REVCMD")]
            silent.append([p for p in patterns
                           if p.strip() and p[:1] not in ("!", "#")])

    typeable = [p for group in silent for p in group]
    typed = [p for p in typeable if types_pattern(row, p)]
    note = ""
    if rollable:
        note += "  ROLLABLE: " + "; ".join(
            l.split("[")[1].split("]")[0] for l in rollable)
    if typeable:
        note += "  SILENT-TYPEABLE: " + "; ".join(typeable[:6])
    if typed:
        note += "  TYPED: " + "; ".join(typed[:6])
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
