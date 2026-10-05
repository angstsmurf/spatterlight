#!/usr/bin/env python3
"""Capture the room pictures of the original ZX Spectrum games from MAME.

    mame_capture.py [--mame DIR] [--jobs N] [GAME...] [GAME:ROOM...]

For every room the interpreter draws a picture for (the room lines of
expected/<row>.txt), the original is started from its snapshot, the player is
put in that room by poking the room number, a command is typed so that the
game draws the room, and the display file ($4000-$5AFF) is saved as
mame/<game>/room<N>.scr. mame_compare.py then compares the interpreter's
pictures with these.

That command is a turn of the game: creatures arrive, doors close, the player
is killed or carried somewhere else. So the flags and the object locations
the picture was drawn from are saved too, as room<N>.state, and the
interpreter draws its picture from the same state.

One MAME run per room: a poked room can kill the player, and every run then
starts from the same state. The runs are headless and unthrottled, about two
seconds each, several at a time.

The screen dumps are the games' pictures, so mame/ is git-ignored like games/.
"""

import argparse
import concurrent.futures
import os
import re
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ZX_CAPTURE = os.path.join(HERE, "..", "..", "..", "scott", "saga", "test", "zx_capture.lua")

# name: the manifest row whose game image and golden are used, the MAME
# machine, the answers to the questions asked before the game starts, the
# address of the room number (flag 0; the object locations follow the flags),
# and a command that makes the game notice that the room has changed.
# Questprobe 3 starts in an office it only leaves when the examiner is talked
# to, and its golden lists no rooms: a room's picture is the picture with its
# number, less one, and the pictures after the 34 rooms are drawn over those.
GAMES = {
    "questprobe-3": dict(row="sna-questprobe-3", machine="spectrum", keys=["", "n", "talk examiner", ""],
                         room=0x5B70, redraw="look", flags=69, rooms=range(1, 35)),
    "rebel-planet": dict(row="sna-rebel-planet", machine="spectrum", keys=["n"], room=0x5D87, redraw="look"),
    "blizzard-pass": dict(row="sna-blizzard-pass", machine="spec128", keys=["y", "n"], room=0x5D87, redraw="look"),
    "heman": dict(row="sna-heman", machine="spectrum", keys=["n"], room=0x5D5C, redraw="i"),
    "temple-of-terror": dict(row="sna-temple-of-terror", machine="spectrum", keys=["n"], room=0x5D5C, redraw="i"),
    "kayleth": dict(row="sna-kayleth", machine="spectrum", keys=["n", "", "n"], room=0x5D5C, redraw="i"),
}

# The original keeps 67 flags ("flags" if not), then a location for each
# object.
FLAGS = 67
OBJECTS = 256


def manifest_file(row):
    for line in open(os.path.join(HERE, "games.manifest.tsv"), encoding="utf-8"):
        f = line.rstrip("\n").split("\t")
        if f[0] == row:
            return os.path.join(HERE, "games", f[1], f[2])
    sys.exit("no manifest row " + row)


def rooms_with_picture(row):
    rooms = []
    for line in open(os.path.join(HERE, "expected", row + ".txt"), encoding="utf-8"):
        m = re.match(r"room(\d+)\s+(?!no picture)", line)
        if m:
            rooms.append(int(m.group(1)))
    return rooms


def capture(mame_dir, name, room):
    game = GAMES[name]
    out = os.path.join(HERE, "mame", name)
    os.makedirs(out, exist_ok=True)
    target, state = "room%d.scr" % room, "room%d.state" % room
    with tempfile.TemporaryDirectory() as tmp:
        scene = os.path.join(tmp, "room.scene")
        with open(scene, "w") as f:
            for key in game["keys"]:
                f.write(("!hw " + key).rstrip() + "\n")
            f.write("!hwpoke %04x=%02x\n%s\n!check %s\n!hwsave %s %04x %d\n"
                    % (game["room"], room, game["redraw"], target, state, game["room"], game.get("flags", FLAGS) + OBJECTS))
        subprocess.run(
            [os.path.join(mame_dir, "mame"), game["machine"], "-snapshot", manifest_file(game["row"]),
             "-autoboot_script", os.path.abspath(ZX_CAPTURE), "-nothrottle", "-video", "none",
             "-sound", "none", "-skip_gameinfo"],
            cwd=mame_dir, env=dict(os.environ, SCENE=scene),
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        if not all(os.path.exists(os.path.join(tmp, f)) for f in (target, state)):
            return name, room, False
        for f in (target, state):
            os.replace(os.path.join(tmp, f), os.path.join(out, f))
    return name, room, True


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--mame", default=os.path.expanduser("~/mame"), help="folder with the mame binary and roms/")
    ap.add_argument("--jobs", type=int, default=8)
    ap.add_argument("what", nargs="*", help="game, or game:room; default all")
    args = ap.parse_args()

    todo = []
    for what in args.what or GAMES:
        name, _, room = what.partition(":")
        if name not in GAMES:
            sys.exit("unknown game %s (have: %s)" % (name, ", ".join(GAMES)))
        if not os.path.exists(manifest_file(GAMES[name]["row"])):
            print("%s: game image not in games/, skipped" % name)
            continue
        todo += [(name, int(room))] if room else [(name, r) for r in GAMES[name].get("rooms") or rooms_with_picture(GAMES[name]["row"])]

    failed = 0
    with concurrent.futures.ThreadPoolExecutor(args.jobs) as pool:
        for name, room, ok in pool.map(lambda t: capture(args.mame, *t), todo):
            if not ok:
                failed += 1
                print("%s room %d: no screen dump" % (name, room))
    print("captured %d screens, %d failed" % (len(todo) - failed, failed))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
