#!/usr/bin/env python3
"""Protocol traces of the four Infocom V6 games, as a regression net for
terps/bocfel/z6.

Each case runs the Spatterlight build of bocfel under v6trace.py and writes
the trace (see there) to OUT/<case>.trace, with what the interpreter wrote to
stderr -- the entrypoints it found, mostly -- in OUT/<case>.err.  Run it once
before a change and once after, and compare:

    run_v6_traces.py --out before
    ... change, rebuild ...
    run_v6_traces.py --out after --compare before

A trace names each picture by a hash of its file, so it tells that a picture
changed but not how.  The pictures themselves are kept in OUT/pictures, and
for every case whose trace changed, --compare goes on to draw the screens of
both runs and compare those (see v6screens.py): "SAME SCREENS" is a change in
how the pictures are sent that nobody could see, "SHIFTED PIXELS" a picture
scaled a fraction of a pixel differently, and "SCREENS DIFFER" a change to
look at.  The last two come with the old and the new screen side by side in
OUT/montages.

The story files and the graphics files of the other platforms are not in the
repository; the paths below are where they live on the machine this was
written on, and so are the saved games the minigame cases start from
(SAVES).  A case whose files are missing is skipped.  The only cases that
need nothing from outside the repository are the "tracked" ones, which run
the .zblorb files of SpatterlightTests.

    --build DIR     where bocfel, libglkimp.dylib and libreadwoz.dylib are
                    (default build/Debug)
    --out DIR       where the traces go (required)
    --compare DIR   an earlier --out directory to compare with
    --jobs N        cases run at once (default 8)
    --list          print the case names and stop
    PATTERN ...     run only the cases whose name contains one of these
"""

import concurrent.futures
import glob
import math
import os
import re
import shutil
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
sys.path.insert(0, HERE)
import v6screens  # noqa: E402
import v6trace  # noqa: E402

HOME = os.path.expanduser("~")
SUPPORT = os.path.join(REPO, "SpatterlightTests", "Supporting Files")
AUTOSAVES = os.path.join(HOME, "Library", "Application Support", "Spatterlight",
                         "Bocfel Files", "Autosaves")
DOWNLOADS = os.path.join(HOME, "Downloads")
# Saved games (Quetzal) at the doors of the Zork Zero minigames.
SAVES = ("/Volumes/Extern/Temp downloads/Interactive Fiction experiments/"
         "glksaves/zork zero saves")

AMIGA, MACBW, APPLE2, VGA, EGA, CGA, BLORB, NONE = range(8)
FORMATS = {AMIGA: "amiga", MACBW: "macbw", APPLE2: "apple2", VGA: "vga",
           EGA: "ega", CGA: "cga", BLORB: "blorb", NONE: "none"}

# Per game: the directory of story files, the pattern of their names, the
# release the long cases use, the graphics files to stage next to the story
# (source name -> the name find_graphics_files() looks for), the command
# script of SpatterlightTests, and the PRNG seed that carries that script
# furthest (the scripts were recorded against other seeds, and a walkthrough
# that goes astray stops reaching the minigames).
GAMES = {
    "zork0": dict(
        dir=os.path.join(DOWNLOADS, "ZorkZeroes"), stories="zork0-*.z6",
        main="zork0-r393-s890714.z6", script="Zork Zero command script.txt",
        seed=1234,
        files={"ZORK0.MG1": "zork0.MG1", "ZORK0.EG1": "zork0.EG1",
               "ZORK0.CG1": "zork0.CG1", "ZORK0.blb": "zork0.blb",
               "CPIC.DATA": "CPIC.DATA", "PIC.DATA": "PIC.DATA",
               "Zork Zero side %d.woz": "zork0 side %d.woz"}),
    "arthur": dict(
        dir=os.path.join(DOWNLOADS, "Arthurs"), stories="arthur-*.z6",
        main="arthur-r74-s890714.z6", script="Arthur command script.txt",
        seed=1234,
        files={"ARTHUR.MG1": "arthur.MG1", "ARTHUR.EG1": "arthur.EG1",
               "ARTHUR.EG2": "arthur.EG2", "ARTHUR.CG1": "arthur.CG1",
               "arthur.blb": "arthur.blb",
               "CPIC.DATA": "CPIC.DATA", "PIC.DATA": "PIC.DATA",
               "Arthur side %d.woz": "arthur side %d.woz"}),
    "journey": dict(
        dir=os.path.join(DOWNLOADS, "Journeys"), stories="journey-*.z6",
        main="journey-r83-s890706.z6", script="Journey command script.txt",
        seed=1234,
        files={"JOURNEY.MG1": "journey.MG1", "JOURNEY.EG1": "journey.EG1",
               "JOURNEY.EG2": "journey.EG2", "JOURNEY.CG1": "journey.CG1",
               "journey.blb": "journey.blb",
               "CPIC.DATA": "CPIC.DATA", "PIC.DATA": "PIC.DATA",
               "Journey side %d.woz": "journey side %d.woz"}),
    "shogun": dict(
        dir=os.path.join(DOWNLOADS, "Shoguns"), stories="shogun-*.z6",
        main="shogun-r322-s890706.z6", script="Shogun command script.txt",
        seed=1234,
        files={"SHOGUN.MG1": "shogun.MG1", "SHOGUN.EG1": "shogun.EG1",
               "SHOGUN.CG1": "shogun.CG1", "shogun.blb": "shogun.blb",
               "Cpic.data": "CPIC.DATA", "Pic.data": "PIC.DATA",
               "Shogun - Side %d.woz": "shogun - Side %d.woz"}),
}

# Hand-written scripts for the screens the walkthroughs never open.  Keys
# are Glk key codes: -6 Return, -4 up, -5 down, -2 left, -3 right, -8 Escape.
UI = (["hint", "key:y"] + ["key:-5", "key:-5", "key:-6", "key:-6", "key:-6",
                           "key:-6", "key:n", "key:-6", "key:m", "key:p",
                           "key:n", "key:n", "key:-6", "key:-6", "key:-4",
                           "mouse:120,60", "key:-6", "key:q", "key:q"]
      + ["look", "map", "key:y", "mouse:200,150", "key:-6", "key:-6",
         "define", "key:-5", "key:x", "key:-6", "mouse:80,40", "key:-4",
         "key:-2", "key:-3"] + ["key:-5"] * 16 + ["key:-6", "key:-8"]
      + ["look", "fkey:1", "fkey:4", "fkey:5", "mode", "look", "inventory",
         "mode", "refresh", "color", "key:-6", "key:-6", "version",
         "$verify", "undo", "notify", "verbose", "brief", "superbrief",
         "look", "score", "time", "diagnose", "restart", "key:y"]
      + ["look", "quit", "key:y"])
JOURNEY_UI = ["key:-6", "key:-5", "key:-5", "key:-4", "key:-3", "key:-6",
              "mouse:100,300", "key:-2", "key:-5", "key:-6", "mouse:300,320",
              "key:-3", "key:-3", "key:-6", "mouse:500,340", "key:-6",
              "key:-8", "key:-5", "key:-6", "key:x", "key:-7", "key:-6",
              "arrange:900x600", "key:-3", "key:-6", "mouse:40,330",
              "key:-6", "key:-5", "key:-6", "key:-6", "key:-3", "key:-5",
              "key:-6", "key:-6"]
EXTRA = {
    "zork0": {"ui": UI}, "arthur": {"ui": UI}, "shogun": {"ui": UI},
    "journey": {"ui": JOURNEY_UI},
}


def minigames(width):
    """{name: (saved game, script)} for the four Zork Zero minigames, played
    with the mouse in a window WIDTH pixels wide showing the Blorb pictures.

    The boxes are where zorkzero.cpp puts them, in the pixels of the 320 pixel
    wide picture; the window shows that picture stretched to its own width.
    Each box is clicked on its first and last pixel, and the pixel outside
    either one is clicked too where a miss shows (it beeps or complains), so
    a hit test that is off by a pixel changes what the game answers."""
    scale = width / 320.0

    def at(x, y, dx=0, dy=0):
        return "mouse:%d,%d,0" % (math.ceil(x * scale) + dx,
                                  math.ceil(y * scale) + dy)

    def first(box):
        return at(box[0], box[1])

    def last(box):
        return at(box[0] + box[2], box[1] + box[3], -1, -1)

    def before(box):
        return at(box[0], box[1], -1, 0)

    def after(box):
        return at(box[0] + box[2], box[1] + box[3], 0, -1)

    # Tower of Bozbar: in the saved game all six weights are on the centre
    # peg, the 1-ugh one on top.  A weight's box is two pixels taller than
    # the step from one row to the next and the lower row is tried first, so
    # the part of it that is its own is ten pixels high.
    def weight(peg, row):
        return (26 + 89 * peg, 40 + 10 * row, 87, 10)

    def peg(n):
        return (26 + 89 * n, 40, 87, 90)

    undo, leave = (88, 22, 62, 9), (167, 22, 62, 9)
    tower = ["restore", "open louvered door",
             before(weight(1, 0)), first(weight(1, 0)), before(peg(0)),
             first(weight(1, 0)), first(peg(0)),
             after(weight(1, 1)), last(weight(1, 1)), after(peg(2)),
             last(weight(1, 1)), last(peg(2)),
             before(undo), first(undo), after(undo), last(undo),
             first(weight(0, 5)), last(peg(2)),
             before(leave), after(leave), last(leave), ""]

    # Peggleboz: the holes are lettered A to U.
    holes = [(142, 30), (166, 30), (154, 37), (142, 44), (166, 44), (127, 52),
             (154, 52), (181, 52), (112, 60), (139, 60), (169, 60), (196, 60),
             (70, 70), (98, 68), (124, 68), (154, 68), (184, 68), (211, 68),
             (238, 70), (83, 76), (225, 76)]

    def hole(letter):
        return holes[ord(letter) - ord("A")] + (12, 14)

    again, moves, out = (17, 20, 62, 9), (17, 32, 62, 9), (17, 44, 62, 9)
    peggleboz = ["restore", "play peggleboz",
                 before(moves), first(hole("Q")), last(hole("G")),
                 last(hole("S")), first(hole("Q")), first(hole("H")),
                 last(hole("R")), first(moves), "", last(moves), "",
                 after(again), last(again), first(hole("Q")),
                 last(hole("G")), first(again),
                 before(out), after(out), first(out)]

    # Snarfem: four piles, and under them the boxes numbered 1 to 9.
    def pile(n):
        return (14 + (n - 1) * 221 // 3, 55, 69, 36)

    def number(n):
        return (21 + 31 * (n - 1), 121, 27, 11)

    snarfem = ["restore", "s", "", "",
               before(number(1)), after(number(1)), last(pile(2)),
               after(number(3)), first(number(3)), "",
               before(pile(4)), first(pile(4)), after(number(9)),
               first(number(1)), "",
               first(pile(1)), last(pile(3)), last(number(1)), ""]

    # Double Fanucci: the plays are in a text grid (window 10; DISCARD is
    # the second line of the first column), the cards are hit-tested by the
    # game itself.  A play wants two clicks, so this is also the test that
    # the grid goes on asking for clicks.
    discard = ["mouse:3,1,10", "mouse:3,1,10"]
    fanucci = ["restore", "sit", "", ""] + discard + [at(20, 64)]
    for x in (112, 165, 218, 271):
        fanucci += [at(x, 64), ""] + discard
    fanucci += ["", "mouse:20,2,10", "mouse:70,3,10", "mouse:20,0,10",
                "mouse:20,0,10"]
    return {"tower": ("open louvered door.qzl", tower),
            "peggleboz": ("play peggleboz.glksave", peggleboz),
            "snarfem": ("snarfem.qzl", snarfem),
            "fanucci": ("fanucci sit at table.qzl", fanucci)}


def stage(work, build):
    """The interpreter where it expects to find its libraries, and each game
    in a directory of its own with every graphics format beside it."""
    app = os.path.join(work, "app")
    os.makedirs(os.path.join(app, "MacOS"))
    os.makedirs(os.path.join(app, "Frameworks"))
    terp = os.path.join(app, "MacOS", "bocfel")
    shutil.copy2(os.path.join(build, "bocfel"), terp)
    for lib in ("libglkimp.dylib", "libreadwoz.dylib"):
        shutil.copy2(os.path.join(build, lib),
                     os.path.join(app, "Frameworks", lib))
    for name, game in GAMES.items():
        dest = os.path.join(work, "games", name)
        os.makedirs(dest)
        game["staged"] = dest
        game["revisions"] = []
        for story in sorted(glob.glob(os.path.join(game["dir"],
                                                   game["stories"]))):
            shutil.copy2(story, dest)
            game["revisions"].append(os.path.basename(story))
        for src, dst in game["files"].items():
            pairs = [(src % n, dst % n) for n in range(1, 6)] \
                if "%d" in src else [(src, dst)]
            for s, d in pairs:
                s = os.path.join(game["dir"], s)
                if os.path.exists(s):
                    shutil.copy2(s, os.path.join(dest, d))
        tracked = os.path.join(SUPPORT, "Games", name + ".zblorb")
        game["tracked"] = None
        if os.path.exists(tracked):
            alone = os.path.join(work, "tracked", name)
            os.makedirs(alone)
            game["tracked"] = shutil.copy2(tracked, alone)
        with open(os.path.join(SUPPORT, game["script"]),
                  encoding="utf-8") as f:
            game["walk"] = v6trace.load_script(f.read())
    return terp


def interleave(script, every, extras):
    """SCRIPT with the entries of EXTRAS dropped in, one every EVERY lines."""
    out = []
    extras = list(extras)
    for index, line in enumerate(script):
        if index and index % every == 0 and extras:
            out.append(extras.pop(0))
        out.append(line)
    return out + extras


def cases():
    """[(name, game, story path, [session, ...])]; a session is (script,
    Tracer options).  Sessions of one case share an autosave, so every
    session after the first starts by autorestoring."""
    out = []
    for name, game in GAMES.items():
        if game["main"] not in game["revisions"]:
            continue
        main = os.path.join(game["staged"], game["main"])
        walk = game["walk"]
        head = walk[:60]

        def add(case, sessions, story=main):
            out.append(("%s-%s" % (name, case), name, story, sessions))

        for fmt, label in FORMATS.items():
            add("walk-" + label, [(walk, dict(graphics=fmt))])
        # Resizing, and switching graphics format and interpreter under way.
        sizes = ["arrange:1000x700", "arrange:640x400", "arrange:1440x900",
                 "arrange:400x300", "arrange:800x600"]
        for fmt, label in FORMATS.items():
            add("resize-" + label,
                [(interleave(head, 6, sizes), dict(graphics=fmt))])
        switches = ["prefs:%d" % f for f in
                    (VGA, APPLE2, CGA, MACBW, EGA, AMIGA, NONE, BLORB)]
        add("switch", [(interleave(head, 5, switches), dict(graphics=BLORB))])
        add("gfx-toggle", [(interleave(head, 8, ["gfx:0", "arrange:900x500",
                                                 "gfx:1", "gfx:0", "gfx:1"]),
                            dict(graphics=BLORB))])
        add("nographics", [(head, dict(graphics=BLORB, graphics_on=0))])
        for interp in (1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11):
            add("interp%d" % interp, [(head, dict(graphics=BLORB,
                                                  interp=interp))])
            add("interp%d-switch" % interp,
                [(interleave(head, 10, ["prefs:%d:%d" % (BLORB, interp),
                                        "prefs:%d:%d" % (CGA, interp),
                                        "prefs:%d:%d" % (APPLE2, interp),
                                        "prefs:%d:0" % AMIGA]),
                  dict(graphics=VGA))])
        add("voiceover", [(head, dict(graphics=BLORB, voiceover=1))])
        for width, height in ((320, 200), (640, 480), (1024, 768),
                              (1920, 1080), (700, 1100)):
            add("size-%dx%d" % (width, height),
                [(head, dict(graphics=BLORB, width=width, height=height)),
                 ])
        # Quit and come back: the second session autorestores.
        for fmt, label in FORMATS.items():
            add("autorestore-" + label,
                [(walk[:30], dict(graphics=fmt)),
                 (walk[30:60], dict(graphics=fmt))])
        add("autorestore-resized",
            [(walk[:30], dict(graphics=BLORB)),
             (walk[30:60], dict(graphics=BLORB, width=1100, height=750))])
        add("autorestore-switched",
            [(walk[:30], dict(graphics=BLORB)),
             (walk[30:60], dict(graphics=CGA)),
             (walk[60:90], dict(graphics=APPLE2, width=640, height=480))])
        for point in (5, 12, 45, 100, 200):
            add("autorestore-at%d" % point,
                [(walk[:point], dict(graphics=BLORB)),
                 (walk[point:point + 20], dict(graphics=BLORB))])
        for revision in game["revisions"]:
            story = os.path.join(game["staged"], revision)
            label = revision.split(".")[0].replace(name + "-", "")
            add("rev-%s" % label, [(walk[:80], dict(graphics=BLORB))], story)
            add("rev-%s-vga" % label,
                [(interleave(walk[:40], 9, ["arrange:1000x600", "prefs:1",
                                            "prefs:2"]),
                  dict(graphics=VGA))], story)
        if game["tracked"]:
            add("tracked", [(walk, dict(graphics=BLORB))], game["tracked"])
            add("tracked-resize",
                [(interleave(head, 6, sizes), dict(graphics=BLORB))],
                game["tracked"])
        # The whole walkthrough again with the window, the graphics format
        # and the interpreter changing under it, and with the game quit and
        # autorestored every so often.
        add("walk-resize", [(interleave(walk, 23, sizes * 12),
                             dict(graphics=BLORB))])
        add("walk-switch", [(interleave(walk, 31, switches * 6),
                             dict(graphics=BLORB))])
        add("walk-vga-resize", [(interleave(walk, 37, sizes * 8),
                                 dict(graphics=VGA))])
        add("walk-apple2-resize", [(interleave(walk, 41, sizes * 8),
                                    dict(graphics=APPLE2))])
        add("walk-gfx-toggle", [(interleave(walk, 29, ["gfx:0", "gfx:1"] * 20),
                                 dict(graphics=BLORB))])
        tall = ["arrange:500x1400", "arrange:1200x1000", "arrange:900x2000",
                "arrange:700x1100"]
        for fmt, label in FORMATS.items():
            if fmt == NONE:
                continue
            add("tall-" + label, [(interleave(head, 6, tall),
                                   dict(graphics=fmt, width=700,
                                        height=1100))])
            add("walk-tall-" + label, [(walk, dict(graphics=fmt, width=640,
                                                   height=1000))])
        # Pictures unseen: the visual puzzles are offered solved or simplified.
        add("walk-nographics", [(walk, dict(graphics=BLORB, graphics_on=0,
                                            affirm="y"))])
        add("walk-nographics-declined", [(walk, dict(graphics=BLORB,
                                                     graphics_on=0,
                                                     affirm="n"))])
        add("walk-voiceover", [(walk, dict(graphics=BLORB, voiceover=1,
                                           affirm="y"))])
        add("walk-sessions", [(walk[at:at + 70], dict(graphics=BLORB))
                              for at in range(0, len(walk), 70)])
        add("walk-sessions-vga", [(walk[at:at + 110], dict(graphics=VGA))
                                  for at in range(0, len(walk), 110)])
        for label, script in EXTRA.get(name, {}).items():
            for fmt in (BLORB, VGA, APPLE2, AMIGA, MACBW, CGA):
                add("%s-%s" % (label, FORMATS[fmt]),
                    [(script, dict(graphics=fmt))])
            add(label + "-ega", [(script, dict(graphics=EGA))])
            # The same screens in a window much taller than the pictures,
            # where the borders have to be extended downwards.
            for fmt, fmt_label in FORMATS.items():
                if fmt != NONE:
                    add("%s-tall-%s" % (label, fmt_label),
                        [(script, dict(graphics=fmt, width=700,
                                       height=1100))])
            add(label + "-resize", [(interleave(script, 7, sizes * 3),
                                     dict(graphics=BLORB))])
            add(label + "-switch", [(interleave(script, 9, switches * 2),
                                     dict(graphics=BLORB))])
    # The Zork Zero minigames by mouse, from saved games.
    game = GAMES["zork0"]
    if game["main"] in game["revisions"]:
        story = os.path.join(game["staged"], game["main"])
        for width, height in ((800, 600), (1000, 700), (430, 600)):
            for label, (save, script) in minigames(width).items():
                save = os.path.join(SAVES, save)
                if os.path.exists(save):
                    out.append(("zork0-mouse-%s-%dx%d" % (label, width, height),
                                "zork0", story,
                                [(script, dict(graphics=BLORB, width=width,
                                               height=height,
                                               savepath=save))]))
    return out


def run_case(terp, work, pictures, case):
    name, game, story, sessions = case
    signature = "v6trace-" + name
    saves = os.path.join(AUTOSAVES, signature)
    shutil.rmtree(saves, ignore_errors=True)
    home = os.path.join(work, "home", name)
    os.makedirs(home, exist_ok=True)
    trace, errors = [], []
    env = {"SPATTERLIGHT_AUTOSAVE_SIGNATURE": signature,
           "BOCFEL_RANDOM_SEED": str(GAMES[game]["seed"]),
           "TMPDIR": home + "/"}
    for index, (script, options) in enumerate(sessions):
        if index:
            trace.append("====== session %d" % (index + 1))
            errors.append("====== session %d\n" % (index + 1))
        lines, rc, err, left = v6trace.run(terp, story, home, script,
                                           env=env, patient=game != "journey",
                                           pictures=pictures, **options)
        trace += lines
        trace.append("=== exit %s, %d script entries left" % (rc, len(left)))
        # Window tags depend on how many windows the earlier, autosaved
        # session went through before the library was written out.
        errors.append(re.sub(r"\btag[: ]\d+", "tag N", err))
    shutil.rmtree(saves, ignore_errors=True)
    shutil.rmtree(home, ignore_errors=True)
    return name, "\n".join(trace) + "\n", "".join(errors)


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
    work = tempfile.mkdtemp(prefix="v6traces-")
    try:
        terp = stage(work, build)
        todo = [c for c in cases()
                if not patterns or any(p in c[0] for p in patterns)]
        if listing:
            print("\n".join(c[0] for c in todo))
            return 0
        pictures = os.path.join(out, "pictures")
        os.makedirs(pictures, exist_ok=True)
        changed, missing, unhealthy = [], [], []
        with concurrent.futures.ThreadPoolExecutor(jobs) as pool:
            for name, trace, err in pool.map(
                    lambda c: run_case(terp, work, pictures, c), todo):
                with open(os.path.join(out, name + ".trace"), "w") as f:
                    f.write(trace)
                with open(os.path.join(out, name + ".err"), "w") as f:
                    f.write(err)
                for line in trace.split("\n"):
                    if line.startswith("=== exit") \
                            and not line.startswith("=== exit 0,"):
                        unhealthy.append("%s: %s" % (name, line))
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
            ran = {c[0] for c in todo}
            names = [n for n in v6screens.changed_cases(compare, out)
                     if n in ran]
            if names:
                v6screens.compare(compare, out, names, jobs=jobs)
            return 1 if changed else 0
        return 0
    finally:
        shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
