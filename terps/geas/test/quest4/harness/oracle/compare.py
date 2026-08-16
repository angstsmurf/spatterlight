#!/usr/bin/env python3
"""Diff each corpus game's geas transcript against QuestViva's V4Game.

The Quest 4 corpus transcripts in ../../goldens are *self*-goldens: they freeze
what geas prints, bugs and all.  This replays the very same command scripts
through QuestViva's V4Game -- a line-by-line C# translation of Axe's VB6 Quest
4, and therefore a real oracle -- and diffs the two, so that a difference is a
place where geas and Quest 4 disagree.

The two harnesses print some things differently even when the engines agree, so
both sides are put through the same normalisation first (see `normalise`), and
qv4 itself already prints menus in geas's shape.  What is left over is engine
behaviour.

Usage:  compare.py [label ...]
Output: a summary table on stdout, and out/<label>.{geas,qv4,diff} per game.
"""
import os
import re
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
Q4 = os.path.abspath(os.path.join(HERE, "..", ".."))
GAMES = os.path.join(Q4, "games")
GOLDENS = os.path.join(Q4, "goldens")
OUT = os.path.join(HERE, "out")
QV4 = os.path.join(HERE, "bin", "Release", "net10.0", "qv4.dll")

# `play LABEL "game" "<title> - command script.txt" "marker" [flags...]`
# The label is tokenised along with everything else because one of them is
# quoted and has a space in it ("The Lazst Resort").
PLAY = re.compile(r'^play\s+(.*)$')


def tokenise(rest):
    """Split a play line's arguments the way sh would (quotes, no expansion)."""
    out, cur, quote = [], "", None
    for ch in rest:
        if quote:
            if ch == quote:
                quote = None
            else:
                cur += ch
        elif ch in "\"'":
            quote = ch
        elif ch.isspace():
            if cur:
                out.append(cur)
                cur = ""
        elif ch == "\\":
            continue
        else:
            cur += ch
    if cur:
        out.append(cur)
    return out


def read_table():
    """Every game in ../run_walkthroughs.sh, in order, with its flags."""
    path = os.path.join(Q4, "harness", "run_walkthroughs.sh")
    text = open(path).read()
    # A play line may be continued with a trailing backslash (World's End).
    text = re.sub(r"\\\n\s*", " ", text)
    games = []
    for line in text.split("\n"):
        m = PLAY.match(line.strip())
        if not m:
            continue
        args = tokenise(m.group(1))
        if len(args) < 4:
            continue
        label, game, script, marker = args[0], args[1], args[2], args[3]
        flags = args[4:]
        games.append(dict(label=label, game=game, script=script,
                          marker=marker, flags=flags))
    return games


def normalise(text, is_geas):
    """Everything both harnesses should agree on, and nothing they should not.

    qv4 already normalises its own side (trailing space, blank runs); this
    repeats it for the geas transcript and applies the one asymmetry left:
    geas's frontend prints a title/version/author banner before the game's
    first output, which is Quest's window title, not game text.
    """
    # Quest's Player is a web view, so its output is HTML: a run of spaces
    # renders as one and leading indentation disappears.  geas prints into a Glk
    # text buffer, where both survive.  That is a frontend difference, not an
    # engine one, so squeeze runs of spaces on both sides rather than reporting
    # every indented line in the corpus.
    lines = [re.sub(r" {2,}", " ", l).strip()
             for l in text.replace("\r\n", "\n").split("\n")]
    out = []
    for l in lines:
        # The runner's own end-of-run summary, not game output.
        if is_geas and l.startswith("[runner] "):
            continue
        # Blank lines go too.  Quest's `Print` ends a paragraph, its Player puts
        # the vertical space in with CSS, and qv4 has to guess a newline back;
        # geas emits its blank lines itself.  Comparing them measures the two
        # frontends, not the two engines, and it drowns out everything else.
        if l:
            out.append(l)
    return out


def drop_banner(geas, name):
    """Drop geas's leading banner line (see normalise) when it is one.

    geas's frontend opens the transcript with `name[, v<version>][ <author>]`;
    Quest puts the same name in the window title and reports it to the player
    through UpdateGameName, which qv4 echoes as `[gamename] ...`.  Matching the
    line against that name -- rather than guessing from its shape -- means a
    game that really does print its own title first (Defenders of Gondor) keeps
    it on both sides.
    """
    if name and geas and geas[0].startswith(name):
        geas = geas[1:]
    return geas


def ensure_movecont_stub():
    """Synthesize games/movecont.lib if the corpus predates it.

    A Certain Oscar !includes movecont.lib, a QDK helper library that was
    never distributed with the game.  Quest fatals on a missing library, so
    qv4 cannot open the game without one on disk; a comment-only stand-in
    defines nothing, which leaves geas playing exactly as it did when it
    skipped the missing include.  fetch_games.sh writes the same stub; this
    is the self-heal for corpora fetched before it did.
    """
    path = os.path.join(GAMES, "movecont.lib")
    if os.path.isdir(GAMES) and not os.path.exists(path):
        with open(path, "w", newline="") as f:
            f.write(
                "' movecont.lib stub: the real QDK library was never"
                " distributed with the game.\r\n"
                "' Both geas and QuestViva read this adjacent file; it defines"
                " nothing, matching\r\n"
                "' geas's historical behaviour of including nothing when the"
                " file was missing.\r\n")


def run(g):
    label = g["label"]
    gamefile = os.path.join(GAMES, g["game"])
    script = os.path.join(GOLDENS, g["script"])
    golden = os.path.join(GOLDENS, g["script"].replace(
        " - command script.txt", " - transcript.txt"))
    if not (os.path.exists(gamefile) and os.path.exists(script)
            and os.path.exists(golden)):
        return label, "SKIP", "missing files", 0
    # A transcript made with --save-scum or --fight is a function of the
    # runner, not the game -- lethal turns are replayed until the RNG
    # cooperates -- so it cannot be reproduced from the script alone and
    # cannot be compared.  No current walkthrough uses either flag (World's
    # End, the one that did, is now scripted turn for turn against the seed-1
    # draw stream); this guard is for the next one.
    if "--save-scum" in g["flags"] or "--fight" in g["flags"]:
        return label, "SKIP", "save-scum replay", 0

    seed = "1"
    if "--seed" in g["flags"]:
        seed = g["flags"][g["flags"].index("--seed") + 1]
    cmd = ["dotnet", QV4]
    if "--tick" in g["flags"]:
        cmd.append("--tick")
    cmd += [gamefile, script]

    env = dict(os.environ, QVH_SEED=seed)
    try:
        p = subprocess.run(cmd, capture_output=True, text=True, env=env,
                           timeout=900)
    except subprocess.TimeoutExpired:
        return label, "TIMEOUT", "qv4 did not finish in 900s", 0
    if p.returncode != 0:
        # An unhandled .NET exception is a message line followed by a stack
        # trace; the message is the useful half.
        msg = ""
        for l in p.stderr.split("\n"):
            l = l.strip()
            if l and not l.startswith(("at ", "[gamename]", "[diag]", "[out]",
                                       "[cmd]", "[step]")):
                msg = l
                break
        return label, "ERROR", msg[:110], 0

    qv = normalise(p.stdout, False)
    name = ""
    for l in p.stderr.split("\n"):
        if l.startswith("[gamename] "):
            name = l[len("[gamename] "):].strip()
            break
    geas = drop_banner(normalise(open(golden, encoding="utf-8",
                                      errors="replace").read(), True), name)

    os.makedirs(OUT, exist_ok=True)
    open(os.path.join(OUT, label + ".geas"), "w").write("\n".join(geas) + "\n")
    open(os.path.join(OUT, label + ".qv4"), "w").write("\n".join(qv) + "\n")
    d = subprocess.run(["diff", "-u",
                        os.path.join(OUT, label + ".geas"),
                        os.path.join(OUT, label + ".qv4")],
                       capture_output=True, text=True)
    open(os.path.join(OUT, label + ".diff"), "w").write(d.stdout)
    # `in ("+", "-")`, not `in "+-"`: the empty string is a substring of every
    # string, so the blank tail split() leaves behind would count as a diff
    # line and no game could ever come out identical.
    moved = sum(1 for l in d.stdout.split("\n")
                if l[:1] in ("+", "-") and l[:2] not in ("++", "--"))
    if moved == 0:
        return label, "SAME", "", 0
    return label, "DIFF", "%d lines" % moved, moved


def main():
    want = set(sys.argv[1:])
    games = [g for g in read_table() if not want or g["label"] in want]
    if not games:
        print("no matching games")
        return 1
    ensure_movecont_stub()
    rows = list(ThreadPoolExecutor(max_workers=8).map(run, games))
    same = sum(1 for r in rows if r[1] == "SAME")
    for label, verdict, note, moved in sorted(rows, key=lambda r: -r[3]):
        if verdict == "SAME":
            continue
        print("%-22s %-8s %s" % (label, verdict, note))
    print("\n%d/%d identical, %d differ, %d skipped/failed"
          % (same, len(rows),
             sum(1 for r in rows if r[1] == "DIFF"),
             sum(1 for r in rows if r[1] not in ("SAME", "DIFF"))))
    return 0


if __name__ == "__main__":
    sys.exit(main())
