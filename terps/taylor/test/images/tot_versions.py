#!/usr/bin/env python3
"""The version menus of Temple of Terror.

  tot_versions.py TESTDIR PROBE HL

Temple of Terror comes as a graphics version and a text-only version with
longer texts, and the interpreter can also combine the two: the pictures of
the one with the text of the other.

  C64           A disk or tape image holding both files asks which version to
                play: 1 graphics, 2 text only, 3 combined. Other keys are
                ignored.

  ZX Spectrum   The two versions are the two sides of the tape. Loading one
                side looks for the other by file name (the letter before the
                extension, a <-> b) and, if it is there and is the other
                version, asks whether to combine them (Y/N). Other keys are
                answered "Please answer Y or N."

The picture goldens and the command script hashes already pin down what every
choice gives. This checks that it is the right thing: that the menu and the
question are there, and only when they should be, that each answer loads the
version it names, and that the combined version has exactly the pictures of
the graphics version and plays exactly like the text-only one. PROBE
(taylor_image_probe) says which game was loaded and lists its pictures; HL
(taylor_hl) plays ../scripts/temple-of-terror.txt, a whole playthrough, for
the text.

The games are found through the manifest of the picture tests. Sets whose
files are missing are skipped, as in the other suites.
"""

import concurrent.futures
import hashlib
import os
import re
import select
import shutil
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                "..", "..", "..", "common_imagetest"))
import run_image_tests as images
import run_script_tests as scripts

SCRIPT = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                      "..", "scripts", "temple-of-terror.txt")

MENU = (b"This %s image contains one version of Temple of Terror with "
        b"pictures, and one without pictures but slightly more text.\n\n"
        b"Please select one:\n1. Graphics version\n2. Text-only version\n"
        b"3. Use pictures from file 1 and text from file 2")
FOUND = (b"Found files for both the text-only version and the graphics "
         b"version of Temple of Terror.\n")
QUESTION = (b"Would you like to use the longer texts from the text-only "
            b"version along with the graphics from the other file? (Y/N) ")
PLEASE = b"Please answer Y or N.\n"

C64_GAMES = {"1": "Temple of Terror C64",
             "2": "Temple of Terror C64 text only version",
             "3": "Temple of Terror C64 combined version"}
ZX_GRAPHICS = "Temple of Terror"
ZX_TEXT_ONLY = "Temple of Terror text only version"
ZX_COMBINED = "Temple of Terror combined version"

# The manifest folder of the two-sided Spectrum tape, and a game that is not
# Temple of Terror, to stand in for the other side.
ZX_DIR = "tzx-temple-of-terror-two-sides"
OTHER_GAME = "tzx-heman"


class Trial:
    """A file and the keys to type, as run_image_tests.run_with_keys and
    run_script_tests.transcript want it."""

    def __init__(self, path, keys):
        self.dir, self.file = os.path.split(os.path.abspath(path))
        self.keys = keys


def probe(binary, path, keys):
    """(game, number of pictures, the pic lines, the whole output)."""
    out = images.run_with_keys(os.path.abspath(binary), Trial(path, keys), "")
    game = re.search(r"^game +(.*)$", out, re.M)
    pictures = re.search(r"^pictures (\d+)$", out, re.M)
    pics = [l for l in out.splitlines() if l.startswith("pic") and not l.startswith("pictures")]
    return (game.group(1) if game else None,
            int(pictures.group(1)) if pictures else None, pics, out)


def play(binary, path, keys):
    return scripts.transcript(binary, SCRIPT, Trial(path, keys), "")


def play_deleting(binary, files, load, keys, prompt):
    """Like play(), on copies of files ({name: source}) in a scratch folder,
    of which load is started. Once prompt has been printed, and while it waits
    for its key, the copies are deleted, and only then are the keys typed."""
    with open(SCRIPT, "rb") as f:
        commands = f.read()
    with tempfile.TemporaryDirectory() as there, tempfile.TemporaryDirectory() as cwd:
        for name, source in files.items():
            shutil.copyfile(source, os.path.join(there, name))
        game = subprocess.Popen([os.path.abspath(binary), os.path.join(there, load)],
                                stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                stderr=subprocess.DEVNULL, cwd=cwd)
        out = b""
        deadline = time.monotonic() + scripts.TIMEOUT
        while prompt not in out:
            ready, _, _ = select.select([game.stdout], [], [], 1)
            chunk = os.read(game.stdout.fileno(), 65536) if ready else b""
            if (ready and not chunk) or time.monotonic() > deadline:
                game.kill()
                game.wait()
                return b"NO PROMPT\n" + out
            out += chunk
        for name in files:
            os.remove(os.path.join(there, name))
        try:
            rest, _ = game.communicate("".join(k + "\n" for k in keys).encode() + commands,
                                       timeout=scripts.TIMEOUT)
        except subprocess.TimeoutExpired:
            game.kill()
            game.wait()
            return b"TIMEOUT\n"
        out = scripts.BLANKS.sub(b"\n\n", scripts.NOISE.sub(b"", out + rest))
        for name in sorted(os.listdir(cwd)):
            with open(os.path.join(cwd, name), "rb") as f:
                data = f.read()
            out += (f"[file {name}: {len(data)} bytes, "
                    f"sha256 {hashlib.sha256(data).hexdigest()}]\n").encode()
        return out


def without_question(text):
    """The transcript with the Spectrum question and its answers taken out,
    and the empty lines that start it."""
    answers = rb"(?:[^\n]*\n" + re.escape(PLEASE) + rb")*[^\n]*\n"
    return re.sub(re.escape(FOUND + QUESTION) + answers, b"", text).lstrip(b"\n")


class Checks:
    def __init__(self):
        self.passed = 0
        self.failed = 0

    def check(self, ok, what):
        if ok:
            self.passed += 1
        else:
            self.failed += 1
            print(f"FAIL    {what}")


def c64_sets(manifest):
    """{dir: {key: row}} for the C64 images holding both versions."""
    sets = {}
    for r in manifest:
        if r.keys in ("1n", "2n", "3n"):
            sets.setdefault(r.dir, {})[r.keys[0]] = r
    return {d: s for d, s in sets.items() if len(s) == 3}


def check_c64(c, pool, opt, games, dir, rows):
    path = os.path.join(games, dir, rows["1"].file)
    with open(path, "rb") as f:
        kind = b"datasette" if f.read(3) == b"C64" else b"disk"
    jobs = {}
    for key in "123":
        jobs["probe" + key] = pool.submit(probe, opt.probe, path, key + "n")
        jobs["play" + key] = pool.submit(play, opt.hl, path, key + "n")
    # Keys that are not 1, 2 or 3 are ignored.
    jobs["probe-other-keys"] = pool.submit(probe, opt.probe, path, "x04!3n")
    jobs["play-other-keys"] = pool.submit(play, opt.hl, path, "x04!3n")
    # The image is read before the menu waits for its key.
    file = os.path.basename(path)
    jobs["play-deleted"] = pool.submit(play_deleting, opt.hl, {file: path}, file,
                                       "3n", MENU[-20:])
    got = {k: j.result() for k, j in jobs.items()}

    for key in "123":
        game, pictures, _, out = got["probe" + key]
        c.check(game == C64_GAMES[key],
                f"{dir}, menu answer {key}: loaded {game!r}, not {C64_GAMES[key]!r}")
        c.check((pictures == 0) == (key == "2"),
                f"{dir}, menu answer {key}: {pictures} pictures")
        c.check(got["play" + key].startswith(MENU % kind),
                f"{dir}, menu answer {key}: the transcript does not start "
                f"with the menu of a {kind.decode()} image")
    _, n1, pics1, _ = got["probe1"]
    _, n3, pics3, _ = got["probe3"]
    c.check(n3 == n1 and pics3 == pics1,
            f"{dir}: the combined version does not have the pictures of the "
            f"graphics version ({n3} and {n1})")
    c.check(got["play3"] == got["play2"],
            f"{dir}: the combined version does not play like the text-only one")
    c.check(got["play1"] != got["play2"],
            f"{dir}: the graphics and text-only versions play alike")
    c.check(got["probe-other-keys"][3] == got["probe3"][3],
            f"{dir}: x, 0, 4 and ! before 3 were not ignored by the probe")
    c.check(got["play-other-keys"] == got["play3"],
            f"{dir}: x, 0, 4 and ! before 3 were not ignored")
    c.check(got["play-deleted"] == got["play3"],
            f"{dir}: deleting the image while the menu waits changes the game")


def check_zx(c, pool, opt, games, side_a, side_b, other):
    """side_a, side_b: the graphics and text-only sides as in the manifest;
    other: a file of another game, or None."""
    a, b = os.path.join(games, ZX_DIR, side_a), os.path.join(games, ZX_DIR, side_b)
    with tempfile.TemporaryDirectory() as scratch:
        def lay_out(name, files):
            """Copy files, {new name: source}, into a folder of their own."""
            folder = os.path.join(scratch, name)
            os.mkdir(folder)
            for new, source in files.items():
                shutil.copyfile(source, os.path.join(folder, new))
            return folder

        alone_a = lay_out("alone-a", {side_a: a})
        alone_b = lay_out("alone-b", {side_b: b})
        # Other names: only the letter before the extension counts.
        renamed = lay_out("renamed", {"tot a.tzx": a, "tot b.tzx": b})
        # The same version twice is not a pair.
        twice = lay_out("twice", {"tot a.tzx": a, "tot b.tzx": a})
        wrong = lay_out("wrong", {"tot a.tzx": a, "tot b.tzx": other}) if other else None

        runs = {
            "A y": (a, "yn"), "A n": (a, "nn"), "B y": (b, "yn"), "B n": (b, "nn"),
            "A Y": (a, "Yn"), "B N": (b, "Nn"), "A x y": (a, "x?yn"),
            "A alone": (os.path.join(alone_a, side_a), "n"),
            "B alone": (os.path.join(alone_b, side_b), "n"),
            "renamed a y": (os.path.join(renamed, "tot a.tzx"), "yn"),
            "renamed b y": (os.path.join(renamed, "tot b.tzx"), "yn"),
            "twice": (os.path.join(twice, "tot a.tzx"), "n"),
        }
        if wrong:
            runs["wrong"] = (os.path.join(wrong, "tot a.tzx"), "n")
        jobs = {}
        for name, (path, keys) in runs.items():
            jobs[name] = (pool.submit(probe, opt.probe, path, keys),
                          pool.submit(play, opt.hl, path, keys))
        # Both sides are read before the question waits for its key.
        both = {side_a: a, side_b: b}
        deleted = {
            "A y": pool.submit(play_deleting, opt.hl, both, side_a, "yn", QUESTION),
            "B y": pool.submit(play_deleting, opt.hl, both, side_b, "yn", QUESTION),
            "A n": pool.submit(play_deleting, opt.hl, both, side_a, "nn", QUESTION),
        }
        got = {n: (p.result(), t.result()) for n, (p, t) in jobs.items()}
        deleted = {n: j.result() for n, j in deleted.items()}

    def game(name):
        return got[name][0][0]

    def pictures(name):
        return got[name][0][1]

    def pics(name):
        return got[name][0][2]

    def text(name):
        return got[name][1]

    expect = {"A y": ZX_COMBINED, "B y": ZX_COMBINED, "A Y": ZX_COMBINED,
              "A x y": ZX_COMBINED, "renamed a y": ZX_COMBINED,
              "renamed b y": ZX_COMBINED,
              "A n": ZX_GRAPHICS, "A alone": ZX_GRAPHICS, "twice": ZX_GRAPHICS,
              "wrong": ZX_GRAPHICS,
              "B n": ZX_TEXT_ONLY, "B N": ZX_TEXT_ONLY, "B alone": ZX_TEXT_ONLY}
    for name in got:
        c.check(game(name) == expect[name],
                f"{ZX_DIR}, {name}: loaded {game(name)!r}, not {expect[name]!r}")

    # The question is asked when the other side is there, and only then.
    for name in got:
        asked = (FOUND + QUESTION) in text(name)
        should = name not in ("A alone", "B alone", "twice", "wrong")
        c.check(asked == should,
                f"{ZX_DIR}, {name}: " + ("asked" if asked else "did not ask")
                + " whether to combine the two sides")
    c.check(PLEASE in text("A x y")
            and text("A x y").count(PLEASE) == 2,
            f"{ZX_DIR}: x and ? were not each answered with {PLEASE!r}")
    for name in ("A y", "B y", "B n", "A n"):
        answer = name[-1].encode()
        c.check((QUESTION + answer + b"\n") in text(name),
                f"{ZX_DIR}, {name}: the answer is not echoed after the question")

    # Pictures: the graphics version's, in the combined version from either
    # side; none in the text-only version.
    c.check(pictures("A n") and pictures("A n") > 0,
            f"{ZX_DIR}: the graphics version has no pictures")
    c.check(pictures("B n") == 0,
            f"{ZX_DIR}: the text-only version has {pictures('B n')} pictures")
    for name in ("A y", "B y", "renamed a y", "renamed b y"):
        c.check(pics(name) == pics("A n"),
                f"{ZX_DIR}, {name}: the pictures are not those of the graphics version")
    c.check(got["A y"][0][3] == got["B y"][0][3],
            f"{ZX_DIR}: the combined version is not the same from either side")
    for name in ("A alone", "twice", "wrong"):
        if name in got:
            c.check(got[name][0][3] == got["A n"][0][3],
                    f"{ZX_DIR}, {name}: not the graphics version of 'A n'")
    c.check(got["B alone"][0][3] == got["B n"][0][3],
            f"{ZX_DIR}, B alone: not the text-only version of 'B n'")

    # Text: the combined version plays like the text-only one, from either
    # side; a side alone plays as it does when the other side is declined.
    text_only = without_question(text("B n"))
    for name in ("A y", "B y", "A Y", "B N", "renamed a y", "renamed b y"):
        c.check(without_question(text(name)) == text_only,
                f"{ZX_DIR}, {name}: does not play like the text-only version")
    c.check(without_question(text("A x y")) == without_question(text("A y")),
            f"{ZX_DIR}, A x y: does not play like A y")
    c.check(without_question(text("A n")) != text_only,
            f"{ZX_DIR}: the graphics and text-only versions play alike")
    for alone, declined in (("A alone", "A n"), ("B alone", "B n"),
                            ("twice", "A n"), ("wrong", "A n")):
        if alone in got:
            c.check(text(alone).lstrip(b"\n") == without_question(text(declined)),
                    f"{ZX_DIR}, {alone}: does not play like {declined}")
    for name, played in deleted.items():
        c.check(played == text(name),
                f"{ZX_DIR}, {name}: deleting both sides while the question "
                f"waits changes the game")


def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__)

    class Opt:
        pass
    opt = Opt()
    testdir, opt.probe, opt.hl = sys.argv[1:]
    games = os.path.join(testdir, "games")
    manifest = [r for r in images.read_manifest(os.path.join(testdir, "games.manifest.tsv"))
                if r.id != images.COMPANION]
    missing, wrong = images.check_corpus(manifest, games)
    for binary in (opt.probe, opt.hl):
        if not os.access(binary, os.X_OK):
            sys.exit(f"{binary} is not built")

    def present(dir):
        return dir not in missing and dir not in wrong

    c = Checks()
    skipped = 0
    with concurrent.futures.ThreadPoolExecutor(os.cpu_count() or 4) as pool:
        sets = c64_sets(manifest)
        for dir, rows in sorted(sets.items()):
            if present(dir):
                check_c64(c, pool, opt, games, dir, rows)
            else:
                skipped += 1

        zx = [r for r in manifest if r.dir == ZX_DIR]
        if not zx:
            sys.exit(f"tot_versions.py: no manifest rows for {ZX_DIR}")
        if present(ZX_DIR):
            graphics = next(r.file for r in zx if r.note.startswith(ZX_GRAPHICS + ";"))
            text_only = next(r.file for r in zx if r.note.startswith(ZX_TEXT_ONLY + ";"))
            other = next((r for r in manifest if r.id == OTHER_GAME and present(r.dir)), None)
            check_zx(c, pool, opt, games, graphics, text_only,
                     os.path.join(games, other.dir, other.file) if other else None)
        else:
            skipped += 1

    print(f"Temple of Terror versions: {c.passed} checks passed, {c.failed} failed, "
          f"{skipped} of {len(sets) + 1} images skipped (game not in games/)")
    return 1 if c.failed else 0


if __name__ == "__main__":
    sys.exit(main())
