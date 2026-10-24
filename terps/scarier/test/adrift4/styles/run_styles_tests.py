#!/usr/bin/env python3
"""What the Spatterlight build of Scarier prints the style fixtures in.

census_expected.txt (`make -f Makefile.headless stylestest`) pins the
assignment glk/os_glk_styles.cpp makes for each fixture; this checks the other
half, through the real Glk frontend: the style hints the terp sets before its
windows open, and the style each span of the introduction and a LOOK lands
in.  Each fixture's report (test/glkdrive.py --styles) is compared with
<fixture>_glk_expected.txt.

Like run_autosave_tests.py, whose terp staging it borrows, it needs the
Spatterlight build of the terp: the headless harnesses compile no os_glk.

Usage:
  python3 run_styles_tests.py [--terp PATH] [--build] [--bless] [substring]
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


def report(terp, game, work):
    """The --styles report of one session: hints in order, then spans."""
    sig = "scarier-styles-test-" + os.path.basename(game)
    os.environ["SPATTERLIGHT_AUTOSAVE_SIGNATURE"] = sig
    rat.clean_autosave(sig)
    d = glkdrive.Driver(terp, game, work, SCRIPT)
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
    return "\n".join(lines) + "\n", problems


def main():
    argv = sys.argv[1:]
    terp, build, bless = None, False, False
    while argv and argv[0].startswith("-"):
        opt = argv.pop(0)
        if opt == "--terp":
            terp = os.path.abspath(argv.pop(0))
        elif opt == "--build":
            build = True
        elif opt == "--bless":
            bless = True
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
        for game in sorted(glob.glob(os.path.join(HERE, "styles_*.taf"))):
            name = os.path.splitext(os.path.basename(game))[0]
            if pattern not in name:
                continue
            got, problems = report(terp, game, work)
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
