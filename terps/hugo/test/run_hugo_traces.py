#!/usr/bin/env python3
"""Protocol traces of Hugo games, as a regression net for terps/hugo/heglk.

Each case runs the Spatterlight build of hugo under the trace driver of the
Bocfel V6 suite (terps/bocfel/test/v6trace.py, on top of
terps/scarier/test/glkdrive.py) and writes every message the interpreter
sends -- window creation and sizing, colours, prints, pictures -- to
OUT/<case>.trace, and what it wrote to stderr to OUT/<case>.err.  hecugel.c
is almost all window layout, so the trace is what a refactor of it must
leave alone.  Run it once before a change and once after, and compare:

    run_hugo_traces.py --out before
    ... change, rebuild ...
    run_hugo_traces.py --out after --compare before

No goldens are stored.  The story files are not in the repository except
Guilty Bastards (UITests); the paths in GAMES are where they live on the
machine this was written on, and a case whose story is missing is skipped.
Each game runs in a directory of its own that links to the files beside
the story, so that its resource files are found and whatever it writes stays
out of the original directory.

    --build DIR     where hugo and libglkimp.dylib are (default build/Debug)
    --out DIR       where the traces go (required)
    --compare DIR   an earlier --out directory to compare with
    --jobs N        cases run at once (default 8)
    --list          print the case names and stop
    PATTERN ...     run only the cases whose name contains one of these
"""

import concurrent.futures
import os
import re
import shutil
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
sys.path.insert(0, os.path.join(REPO, "terps", "bocfel", "test"))
import v6trace  # noqa: E402

HOME = os.path.expanduser("~")
VBOX = os.path.join(HOME, "Delad Virtual Box-mapp", "Hugo games")
IFDOCS = os.path.join(HOME, "Documents", "Interactive Fiction", "Hugo games")
EXPERIMENTS = os.path.join(HOME, "Desktop", "Interactive Fiction experiments")
DOWNLOADS = os.path.join(HOME, "Downloads")
UITESTS = os.path.join(REPO, "UITests", "Supporting Files")
EXTERN = "/Volumes/Extern"
STORYFILES = os.path.join(EXTERN, "Temp downloads", "Story files")

# name -> (story file, command script, or None for the generic one, or a list
# of lines to start the generic one with)
GAMES = {
    "guilty": (os.path.join(UITESTS, "Games", "guilty.hex"),
               os.path.join(UITESTS, "Command scripts",
                            "Guilty Bastards command script.txt")),
    "enceladus": (os.path.join(IFDOCS, "ENCELADUS by Robb Sherwin",
                               "enceladus.hex"), None),
    "tetris": (os.path.join(IFDOCS, "Hugo Tetris", "TETRIS.HEX"), None),
    "scourge": (os.path.join(VBOX, "scourge", "scourge.hex"), None),
    "htgessay": (os.path.join(VBOX, "htgessay.hex"), None),
    "hugozork": (os.path.join(VBOX, "hugozork.hex"), None),
    "cb2": (os.path.join(VBOX, "cb2.hex"), None),
    "clockworkboy": (os.path.join(VBOX, "clockworkboy.hex"), None),
    "colossal": (os.path.join(VBOX, "colossal.hex"), None),
    "memory": (os.path.join(VBOX, "memory.hex"), None),
    "pirate": (os.path.join(VBOX, "pirate.hex"), None),
    "spur": (os.path.join(VBOX, "spur.hex"), None),
    "storm": (os.path.join(VBOX, "storm.hex"), None),
    "tales": (os.path.join(VBOX, "tales_ts.hex"), None),
    "award": (os.path.join(VBOX, "AWARD.HEX"), None),
    "hammurabi": (os.path.join(VBOX, "hammurabi", "hammurabi.hex"), None),
    "ndrift": (os.path.join(VBOX, "ndrift", "ndrift.hex"), None),
    "trading": (os.path.join(VBOX, "trading", "trading.hex"), None),
    "distress": (os.path.join(VBOX, "Ny mapp med objekt", "distress.hex"),
                 None),
    "down": (os.path.join(VBOX, "Ny mapp med objekt", "down.hex"), None),
    "futureboy": (os.path.join(VBOX, "Future Boy! demo", "game",
                               "future.hex"), None),
    "waiting": (os.path.join(DOWNLOADS, "waiting", "waiting.hex"), None),
    "horror1": (os.path.join(DOWNLOADS, "horror", "horrorpartI.hex"), None),
    "horror2": (os.path.join(DOWNLOADS, "horror", "horrorpartII.hex"), None),
    "dragon": (os.path.join(DOWNLOADS, "hugo", "dragon.hex"), None),
    "dawn": (os.path.join(DOWNLOADS, "dawn", "dawn.hex"), None),
    "nextday": (os.path.join(DOWNLOADS, "nextday", "nextday.hex"), None),
    "trollface": (os.path.join(EXPERIMENTS, "trollface", "Trollface.hex"),
                  None),
    "speedo": (os.path.join(EXPERIMENTS, "SpeedIF_YIs1OverX", "SpeedoNew.hex"),
               None),
    "east": (os.path.join(EXPERIMENTS, "hugov31_dos32bit", "EAST.HEX"), None),
    "minimal": (os.path.join(REPO, "babel", "test", "hugo", "minimal.hex"),
                None),
    "czk": (os.path.join(EXTERN, "Cryptozookeeper", "czk.hex"), None),
    "clock": (os.path.join(STORYFILES, "clock.hex"), None),
    "nmnl": (os.path.join(STORYFILES, "NMNL.hex"), None),
    # Both ask whether a Glk interpreter has a status bar.
    "party": (os.path.join(STORYFILES, "party.hex"), ["yes"]),
    "worldbuilder": (os.path.join(STORYFILES, "worldbuilder.hex"), ["yes"]),
    "destiny": (os.path.join(STORYFILES, "SpeedIF04", "DESTINY.HEX"), None),
}

# Keys are Glk key codes: -6 Return, -4 up, -5 down, -2 left, -3 right,
# -8 Escape.  A key: entry nobody asks for is dropped (and noted).
MENU = ["key:-5", "key:-5", "key:-6", "key:-6", "key:-4", "key:-6",
        "key:-8", "key:-8", "key:q", "key:q", "key:-6"]
GENERIC = (["", "", "", "look", "inventory", "examine me", "score"]
           + ["help"] + MENU + ["hint"] + MENU
           + ["north", "south", "east", "west", "up", "down", "look",
              "get all", "drop all", "verbose", "look", "brief", "wait",
              "z", "undo", "again", "save", "look", "restore", "look",
              "about", "credits", "key:-6", "key:-6", "look", "quit", "key:n",
              "look"])
SIZES = ["arrange:1000x700", "arrange:480x360", "arrange:1440x900",
         "arrange:320x240", "arrange:800x600"]


class Tracer(v6trace.Tracer):
    """A sound reaches the app as a file name; trace what is in the file,
    as for a picture."""

    def describe(self, cmd, a1, a2, a3, a4, a5, payload):
        if cmd == v6trace.glkdrive.LOADSOUND:
            path = payload.decode("utf-8", "replace").rstrip("\0")
            return "LOADSOUND %d %d %d file %s" % (
                a1, a2, a3, v6trace.file_digest(path)[0])
        return super().describe(cmd, a1, a2, a3, a4, a5, payload)


def scrub(err):
    """ERR without what changes from run to run: process ids, addresses."""
    err = re.sub(r"==\d+==", "==PID==", err)
    return re.sub(r"0x[0-9a-f]{6,}", "0xADDR", err)


def interleave(script, every, extras):
    """SCRIPT with the entries of EXTRAS dropped in, one every EVERY lines."""
    out = []
    extras = list(extras)
    for index, line in enumerate(script):
        if index and index % every == 0 and extras:
            out.append(extras.pop(0))
        out.append(line)
    return out + extras


def stage(work, build):
    """The interpreter where it expects to find its library."""
    app = os.path.join(work, "app")
    os.makedirs(os.path.join(app, "MacOS"))
    os.makedirs(os.path.join(app, "Frameworks"))
    terp = os.path.join(app, "MacOS", "hugo")
    shutil.copy2(os.path.join(build, "hugo"), terp)
    shutil.copy2(os.path.join(build, "libglkimp.dylib"),
                 os.path.join(app, "Frameworks", "libglkimp.dylib"))
    return terp


def cases():
    """[(name, game, script, Tracer options)]"""
    out = []
    for game, (story, script) in GAMES.items():
        if not os.path.exists(story):
            continue
        if isinstance(script, list):
            walk, script = script + GENERIC, None
        elif script:
            with open(script, encoding="utf-8") as f:
                walk = v6trace.load_script(f.read())
        else:
            walk = GENERIC
        head = walk[:40]

        def add(case, script, **options):
            out.append(("%s-%s" % (game, case), game, script, options))

        add("walk", walk)
        add("resize", interleave(head, 5, SIZES))
        add("nographics", head, graphics_on=0)
        for width, height in ((320, 200), (640, 480), (1440, 900),
                              (600, 1000)):
            add("size-%dx%d" % (width, height), head, width=width,
                height=height)
        if script:
            add("walk-resize", interleave(walk, 17, SIZES * 6))
    return out


def link_game(story, dest):
    """DEST holding a link to every file beside STORY; the story's path in
    DEST."""
    os.makedirs(dest)
    source = os.path.dirname(story)
    for entry in os.listdir(source):
        os.symlink(os.path.join(source, entry), os.path.join(dest, entry))
    return os.path.join(dest, os.path.basename(story))


def run_case(terp, work, case):
    name, game, script, options = case
    home = os.path.join(work, "home", name)
    os.makedirs(home)
    story = link_game(GAMES[game][0], os.path.join(home, "game"))
    env = {"TMPDIR": home + "/"}
    lines, rc, err, left = v6trace.run(
        terp, story, home, script, tracer=Tracer, env=env,
        savepath=os.path.join(home, "saved.sav"), **options)
    trace = lines + ["=== exit %s, %d script entries left" % (rc, len(left))]
    shutil.rmtree(home, ignore_errors=True)
    return name, "\n".join(trace) + "\n", scrub(err)


def first_difference(a, b):
    a, b = a.split("\n"), b.split("\n")
    for index, (x, y) in enumerate(zip(a, b)):
        if x != y:
            return "line %d:\n    - %s\n    + %s" % (index + 1, x[:160],
                                                    y[:160])
    return "length %d -> %d lines" % (len(a), len(b))


def main():
    argv = sys.argv[1:]
    build = os.path.join(REPO, "build", "Debug")
    out = compare = None
    jobs, listing, patterns = 8, False, []
    while argv:
        arg = argv.pop(0)
        if arg == "--build":
            build = argv.pop(0)
        elif arg == "--out":
            out = argv.pop(0)
        elif arg == "--compare":
            compare = argv.pop(0)
        elif arg == "--jobs":
            jobs = int(argv.pop(0))
        elif arg == "--list":
            listing = True
        elif arg.startswith("--"):
            sys.exit(__doc__)
        else:
            patterns.append(arg)
    if not out and not listing:
        sys.exit(__doc__)
    todo = [c for c in cases()
            if not patterns or any(p in c[0] for p in patterns)]
    if listing:
        print("\n".join(c[0] for c in todo))
        return 0
    os.makedirs(out, exist_ok=True)
    work = tempfile.mkdtemp(prefix="hugotraces-")
    try:
        terp = stage(work, build)
        changed, missing, unhealthy = [], [], []
        with concurrent.futures.ThreadPoolExecutor(jobs) as pool:
            for name, trace, err in pool.map(
                    lambda c: run_case(terp, work, c), todo):
                with open(os.path.join(out, name + ".trace"), "w") as f:
                    f.write(trace)
                with open(os.path.join(out, name + ".err"), "w") as f:
                    f.write(err)
                last = trace.rstrip("\n").split("\n")[-1]
                if not last.startswith("=== exit 0,"):
                    unhealthy.append("%s: %s" % (name, last))
                if not compare:
                    continue
                for ext, new in ((".trace", trace), (".err", err)):
                    path = os.path.join(compare, name + ext)
                    if not os.path.exists(path):
                        missing.append(name + ext)
                        continue
                    with open(path) as f:
                        old = f.read()
                    if old != new:
                        changed.append("%s%s: %s" % (name, ext,
                                                     first_difference(old,
                                                                      new)))
        print("%d cases" % len(todo))
        for line in unhealthy:
            print("ABNORMAL EXIT " + line)
        if compare:
            for name in missing:
                print("NEW      " + name)
            for line in changed:
                print("CHANGED  " + line)
            print("%d changed, %d new" % (len(changed), len(missing)))
            return 1 if changed else 0
        return 0
    finally:
        shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
