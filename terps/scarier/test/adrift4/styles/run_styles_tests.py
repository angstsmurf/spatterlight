#!/usr/bin/env python3
"""What the Spatterlight build of Scarier prints the style fixtures in.

census_expected.txt (`make -f Makefile.headless stylestest`) pins the
assignment glk/os_glk_styles.cpp makes for each fixture; this checks the other
half, through the real Glk frontend: the style hints the terp sets before its
windows open, and the style each span of the introduction and a LOOK lands
in.  Each fixture's report (test/glkdrive.py --styles) is compared with
<fixture>_glk_expected.txt.  So is one ADRIFT 5 game from the corpus, Death
Shack, whose centred title cards are <font size> spans (skipped when the
corpus is not fetched: test/adrift5/games is a symlink).

Like run_autosave_tests.py, whose terp staging it borrows, it needs the
Spatterlight build of the terp: the headless harnesses compile no os_glk.

Usage:
  python3 run_styles_tests.py [--terp PATH] [--build] [--bless] [substring]

--switch runs the fixtures with the theme changing between light and dark
before the first command and after each instead, and fails a fixture that
then sends, or prints in, a colour of the side the theme left (see
glkdrive.py); the reports are not compared.
"""

import glob
import os
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(os.path.dirname(HERE), "harness"))
import run_autosave_tests as rat  # noqa: E402
glkdrive = rat.glkdrive

SCRIPT = ["look"]
A5_GAMES = [("deathshack", os.path.join(os.path.dirname(os.path.dirname(HERE)),
                                        "adrift5", "games", "DeathShack.taf"))]


def with_switches(script):
    """The script with the theme going dark and light again before the
    first command and after each (glkdrive's dark: entries)."""
    out = ["dark:1", "dark:0"]
    for entry in script:
        out += [entry, "dark:1", "dark:0"]
    return out


def report(terp, game, work, switch=False):
    """The --styles report of one session: hints in order, then spans."""
    sig = "scarier-styles-test-" + os.path.basename(game)
    os.environ["SPATTERLIGHT_AUTOSAVE_SIGNATURE"] = sig
    rat.clean_autosave(sig)
    d = glkdrive.Driver(terp, game, work,
                        with_switches(SCRIPT) if switch else SCRIPT)
    try:
        rc, err = d.run()
    finally:
        rat.clean_autosave(sig)
    lines = list(d.hints)
    lines += ["SPAN %d %s %r" % (peer, glkdrive.name_of(glkdrive.STYLE_NAMES,
                                                         style), text)
              for peer, style, text in d.spans]
    problems = []
    if rc != 0:
        problems.append("exit %s" % rc)
    if d.script:
        problems.append("script not consumed: %r" % d.script)
    if err.strip():
        problems.append("stderr: " + err.strip()[-400:])
    problems += ["stale colour: " + text for text in d.stale]
    return "\n".join(lines) + "\n", problems


def main():
    argv = sys.argv[1:]
    terp, build, bless, switch = None, False, False, False
    while argv and argv[0].startswith("-"):
        opt = argv.pop(0)
        if opt == "--terp":
            terp = os.path.abspath(argv.pop(0))
        elif opt == "--build":
            build = True
        elif opt == "--bless":
            bless = True
        elif opt == "--switch":
            switch = True
        else:
            sys.exit(__doc__)
    pattern = argv[0] if argv else ""
    if build:
        rat.xcodebuild()

    failed = 0
    with tempfile.TemporaryDirectory() as stage, \
            tempfile.TemporaryDirectory() as work:
        if terp is None:
            terp = rat.stage_default_terp(stage)
        games = [(os.path.splitext(os.path.basename(game))[0], game)
                 for game in sorted(glob.glob(os.path.join(HERE,
                                                           "styles_*.taf")))]
        for name, game in games + A5_GAMES:
            if pattern not in name:
                continue
            if not os.path.exists(game):
                print("%-20s skipped (no %s)" % (name, game))
                continue
            got, problems = report(terp, game, work, switch)
            if switch:
                failed += bool(problems)
                print("%-20s %s" % (name, "FAIL" if problems else "ok"))
                for p in problems:
                    print("    " + p)
                continue
            expected = os.path.join(HERE, name + "_glk_expected.txt")
            if bless and not problems:
                with open(expected, "w", encoding="utf-8") as f:
                    f.write(got)
                print("%-20s blessed" % name)
                continue
            if not problems:
                try:
                    with open(expected, encoding="utf-8") as f:
                        want = f.read()
                except FileNotFoundError:
                    want = None
                if want is None:
                    problems.append("no %s (--bless)"
                                    % os.path.basename(expected))
                elif want != got:
                    problems.append("report differs from %s"
                                    % os.path.basename(expected))
                    import difflib
                    problems += [l.rstrip("\n") for l in difflib.unified_diff(
                        want.splitlines(True), got.splitlines(True),
                        "expected", "got")]
            failed += bool(problems)
            print("%-20s %s" % (name, "FAIL" if problems else "ok"))
            for p in problems:
                print("    " + p)
    print("%d failed" % failed if failed else "all passed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
