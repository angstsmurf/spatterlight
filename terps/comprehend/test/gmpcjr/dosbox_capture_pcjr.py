#!/usr/bin/env python3
"""Capture PCjr 16-colour ground truth for every room, from the real game.

Boots NOVEL1.EXE in DOSBox-X as a PCjr, then shows each room in turn without
playing the game: the current-room byte is poked in the emulator's memory and
the game is made to redraw. Each screenshot's 280x160 picture window is written
to fixtures/captures/ under the name of the pictures the interpreter draws for
that room (the headless `#room` command lists them), and test_gmpcjr_pics then
decides: a capture it reproduces pixel for pixel stays as a fixture, the rest
are moved to fixtures/captures_rejected/ to be looked at.

Usage: dosbox_capture_pcjr.py tr|cc      (run from terps/comprehend, after
                                          `make test` and the headless build)

Two ways of redrawing are used, because neither covers everything:

  look     poke the room, type LOOK. Takes no route through the map, but the
           game then draws the room without most of its objects, and Crimson
           Crown has no bare LOOK at all.
  walk in  poke a neighbouring room, type the direction that leads to the
           target. Objects are drawn; rooms nothing leads into, or whose way in
           the game blocks, are missed.

A rejected capture is therefore not necessarily a renderer bug: first see
whether the game was simply in another state than assumed (an object absent or
present, a blocked exit, the Transylvania werewolf turning up).

Things that do not work, so nobody tries them again: toggling the picture off
and on with a bare Return to force a redraw (from the second switch to the
picture view on, DOSBox-X's PCjr screen shows garbage until the game next
draws a room), and DOSBox-X savestates (loading one does not bring the game
back).

Needs the dosbox-mcp package and a DOSBox-X build with the remote debugger;
their locations and the games directory are the constants below.
"""
import glob
import hashlib
import os
import re
import subprocess
import sys
import time

DOSBOX_MCP = "/Users/administrator/Downloads/dosbox-mcp"
os.environ.setdefault("DOSBOX_X_BIN", "/Users/administrator/dosbox-x-remotedebug/src/dosbox-x")
os.environ.setdefault("DOSBOX_DEBUG_PY_DIR", "/Users/administrator/dosbox-x-remotedebug/tests/integration")
GAMES_DIR = "/Users/administrator/Downloads/comprehend games"

HERE = os.path.dirname(os.path.abspath(__file__))
TERP = os.path.dirname(os.path.dirname(HERE))
CAPTURES = os.path.join(HERE, "fixtures", "captures")
REJECTED = os.path.join(HERE, "fixtures", "captures_rejected")
WORK = os.path.join(HERE, "fixtures", "capture_work")

# key -> game folder, headless game id, data file, answers to the opening
# prompts, linear address of the current-room byte, whether LOOK redraws.
# The address was found by dumping the first 640K at three known rooms and
# keeping the bytes that held the three room numbers; of the candidates it is
# the one whose poke changes where the next move starts from. It holds for a
# game folder mounted as A: with the configuration written below.
GAMES = {
    "tr": ("transylvania", "transylvania", "TR.GDA", ["adventurer", "nobody"], 0xD999, True),
    "cc": ("crimson-crown", "crimsoncrown", "CC1.GDA", [], 0xE7B9, False),
}
DIRECTIONS = ["n", "s", "e", "w", "u", "d", "in", "out"]
WAYS_IN = 2          # neighbours to try per room
QUIET = 8            # unchanged screenshots in a row that count as "drawn"


def interpreter_rooms(game_id, gda, answers):
    """{room: (exits[8], [picture numbers])} from the headless interpreter."""
    script = os.path.join(WORK, "rooms.script")
    with open(script, "w") as f:
        f.write("".join(a + "\n" for a in answers))
        f.write("".join("#room %d\n" % r for r in range(1, 256)))
    out = subprocess.run([os.path.join(TERP, "comprehend_hl"), "-g", game_id, gda],
                         env=dict(os.environ, COMPREHEND_SCRIPT=script),
                         capture_output=True, text=True, errors="replace").stdout
    rooms = {}
    for m in re.finditer(r"Room (\d+) of \d+; exits ([\d ]+); pictures ([\d ]+)\.", out):
        rooms[int(m.group(1))] = ([int(x) for x in m.group(2).split()],
                                  [int(x) for x in m.group(3).split()])
    return rooms


def picture_name(n):
    """Picture number -> <FILE>.<EXT>.<index>, as test_gmpcjr_pics wants it."""
    return "%s%c.MS1.%d" % ("R" if n < 512 else "O", ord("A") + (n % 512) // 16, n % 16)


def main():
    if len(sys.argv) != 2 or sys.argv[1] not in GAMES:
        sys.exit(__doc__)
    folder, game_id, gda, answers, room_addr, has_look = GAMES[sys.argv[1]]
    game_dir = os.path.join(GAMES_DIR, folder)
    for d in (CAPTURES, REJECTED, WORK):
        os.makedirs(d, exist_ok=True)
    rooms = interpreter_rooms(game_id, os.path.join(game_dir, gda), answers)
    if not rooms:
        sys.exit("no rooms: is comprehend_hl built (make -f Makefile.headless)?")

    sys.path.insert(0, DOSBOX_MCP)
    sys.path += glob.glob(DOSBOX_MCP + "/.venv/lib/python*/site-packages")
    from dosbox_mcp import tools as T
    from PIL import Image

    shot_path = os.path.join(WORK, "shot.png")

    def screen():
        for _ in range(5):          # it fails now and then; a retry gets it
            r = T.dosbox_screenshot(shot_path)
            if r.get("ok"):
                break
            time.sleep(1)
        else:
            sys.exit("screenshot failed: %r" % r)
        return Image.open(shot_path).convert("RGB")

    def settle(limit=60):
        # Only the top 160 rows count: the text cursor below them blinks.
        last, same, t0 = None, 0, time.time()
        while time.time() - t0 < limit:
            time.sleep(0.4)
            h = hashlib.md5(screen().crop((0, 0, 320, 160)).tobytes()).hexdigest()
            same = same + 1 if h == last else 0
            last = h
            if same >= QUIET:
                return
        print("  (screen never settled)")

    def command(text):
        T.dosbox_type(text + "\n", delay_ms=40)
        settle()

    def poke_room(room):
        T.dosbox_write_memory(hex(room_addr), "%02x" % room)

    def in_picture_view():
        # The all-text view is two colours.
        return len(screen().crop((0, 0, 320, 160)).getcolors(999) or [0] * 3) > 2

    def show(room, text):
        # Poke and type. The game sometimes falls back to its all-text view; a
        # bare Return brings the picture view back (with garbage on it), and
        # doing the move again then draws the picture cleanly.
        for _ in range(3):
            poke_room(room)
            command(text)
            if in_picture_view():
                return
            T.dosbox_press_key("ret")
            settle()

    candidates = 0

    def save(pictures):
        # Several shots may claim one name (an earlier run's fixture included);
        # keep them all (.alt<n>) so that a shot taken in the wrong game state
        # cannot hide a good one.
        nonlocal candidates
        if not isinstance(pictures, str):
            pictures = "+".join(picture_name(p) for p in pictures)
        base = os.path.join(CAPTURES, folder + "." + pictures)
        k = 0
        while os.path.exists(base + (".ppm" if k == 0 else ".alt%d" % k)):
            k += 1
        with open(base + (".ppm" if k == 0 else ".alt%d" % k), "wb") as f:
            f.write(b"P6\n280 160\n255\n" + screen().crop((20, 0, 300, 160)).tobytes())
        candidates += 1

    conf = os.path.join(WORK, "pcjr.conf")
    with open(conf, "w") as f:
        # captures=: DOSBox-X keeps its own copy of every screenshot there,
        # and would otherwise start a capture/ folder wherever this is run.
        f.write('[dosbox]\nmachine=pcjr\ncaptures=%s\n[cpu]\ncycles=20000\n[autoexec]\n'
                'mount a: "%s" -t floppy\na:\n' % (os.path.join(WORK, "dosbox"), game_dir))
    r = T.dosbox_start(conf_path=conf)
    if not r.get("ok"):
        sys.exit("cannot start DOSBox-X: %r" % r)
    try:
        time.sleep(3)
        T.dosbox_type("novel1.exe\n")
        settle()
        save(sys.argv[1].upper() + "TITLE.MS1.-1")
        T.dosbox_press_key("spc")
        settle()
        for a in answers:
            command(a)
        # The game starts in its all-text view; a bare Return switches to the
        # picture view (and back), and may need a second try to register.
        for _ in range(4):
            if in_picture_view():
                break
            T.dosbox_press_key("ret")
            settle()

        if has_look:
            for room in sorted(rooms):
                show(room, "look")
                pictures = rooms[room][1]
                save(pictures[:1])
                if len(pictures) > 1:
                    save(pictures)
                print("look  room %d" % room, flush=True)
        for room in sorted(rooms):
            ways = [(m, DIRECTIONS[d]) for m in sorted(rooms) for d in range(8)
                    if rooms[m][0][d] == room and m != room]
            for m, d in ways[:WAYS_IN]:
                show(m, d)
                save(rooms[room][1])
                print("walk  room %d from %d going %s" % (room, m, d), flush=True)
    finally:
        T.dosbox_stop()

    # Let the test sort them: promote the next alternative of a failing
    # capture, and reject it for good when there is none left.
    rejected = []
    while True:
        out = subprocess.run(["./test/test_gmpcjr_pics"], cwd=TERP,
                             capture_output=True, text=True).stdout
        fails = re.findall(r"FAIL  capture (.*\.ppm): (.*)", out)
        if not fails:
            break
        for name, why in fails:
            path = os.path.join(CAPTURES, name)
            k = 0
            while os.path.exists(os.path.join(REJECTED, "%s.%d" % (name, k))):
                k += 1
            os.rename(path, os.path.join(REJECTED, "%s.%d" % (name, k)))
            alts = sorted(glob.glob(path[:-4] + ".alt*"))
            if alts:
                os.rename(alts[0], path)
            else:
                rejected.append((name, why))
    for alt in glob.glob(os.path.join(CAPTURES, "*.alt*")):
        os.remove(alt)
    print("%d shots taken; %s" % (candidates, out.strip().splitlines()[-1]))
    for name, why in rejected:
        print("  rejected %s: %s" % (name, why))


if __name__ == "__main__":
    main()
