#!/usr/bin/env python3
# Writes the scene tests (see scenetest.c) that show every picture of the C64
# all-in-one releases of Pirate Adventure and Voodoo Castle (saga/c64_small.c):
# groundtruth_scene/pirate_pictures_c64/ and voodoo_pictures_c64/, one scene
# per picture, next to the walked scenes of pirate_c64/ and voodoo_c64/, whose
# disk images they use. The seas_scenes.py of these games.
#
#   aio_scenes.py               write the scenes
#
# and then, for the goldens (see groundtruth_scene/README.md):
#
#   for s in groundtruth_scene/pirate_pictures_c64/*.scene; do
#       ./c64_capture.py $s; done
#
# The first capture loads the game and leaves a snapshot of the machine
# (`!hwsnap`); the others start from it and take seconds.
#
# Each scene starts the game and writes the room, and what has to be in it,
# straight into the game state: with `!hwpoke` in the original, and with
# `!room` and `!item` in the interpreter. A LOOK then draws the room on both.
#
#   roomNN       every room, with what is in it as the game starts
#   itemNN       an object with a picture that is not in play as the game
#                starts, put in a room and drawn there
#
# The originals keep the room number at $1022 and the item locations as a word
# each from $103c, item 0 first (found by diffing `!hwmem` dumps taken before
# and after a move and a pickup); both number the rooms and items as the
# interpreter does. The originals draw the picture one line lower than the
# interpreter, hence dy=-1.

import os

HERE = os.path.dirname(os.path.abspath(__file__))

ROOM = 0x1022
ITEMS = 0x103c

GAMES = {
    "pirate": dict(
        title="Pirate Adventure", game="../pirate_c64/pirate.d64", rooms=26,
        # Items with a picture that are not in play as the game starts, and
        # the room each is drawn in: the secret passage, which has no items
        # of its own, unless the original only draws it in one room (the gate
        # lists of c64_small.c).
        items={3: 3, 4: 3, 7: 3, 9: 3, 10: 3, 19: 3, 26: 1, 28: 9, 37: 6, 41: 4,
               42: 3, 63: 3},
        # Items the interpreter draws as the likeness of another (the
        # pirate_extras of c64_small.c: the bottles like the bottle of rum,
        # the lit torch like the unlit one) where the original draws
        # nothing. The check gets this min= instead of being exact.
        likeness={7: 99, 42: 99, 9: 99, 63: 99},
        # What a room needs so that the LOOK, a turn like any other, does
        # not end elsewhere: (item, where) pairs.
        setup={5: [(44, 255)],    # the ledge: carry the sneakers or fall
               10: [(54, 255)]},  # the ocean: carry the water wings or drown
        carried=255,
    ),
    "voodoo": dict(
        title="Voodoo Castle", game="../voodoo_c64/voodoo.d64", rooms=25,
        # Drawn in room 24, the one room with nothing in it.
        items={6: 24, 23: 24, 28: 24, 31: 24, 34: 24, 37: 24, 41: 24},
        likeness={},
        # The lab: carry the shield, or the chem tubes put you in a lot of
        # TROUBLE (room 25) three turns in ten.
        setup={17: [(7, 255)]},
        carried=255,
    ),
}

START = ["!hwwait 40", "!hw n"]


def scene(folder, g, name, what, room, moves=(), item=None):
    lines = [f"# {g['title']}: {what}", "# (written by aio_scenes.py)"]
    check = f"!check {name}.vic dy=-1"
    if item in g["likeness"]:
        lines += ["# The original draws nothing for this item; the interpreter",
                  "# draws it as the likeness of another (c64_small.c, pirate_extras)."]
        check += f" min={g['likeness'][item]}"
    lines += [f"!game {g['game']}"] + START + [f"!hwsnap {os.path.basename(folder)[:6]}.vsf"]
    for it, to in list(g["setup"].get(room, [])) + list(moves):
        lines += [f"!hwpoke {ITEMS + 2 * it:04x}={to:02x}", f"!item {it} {to}"]
    lines += [f"!hwpoke {ROOM:04x}={room:02x}", f"!room {room}", "LOOK", check, ""]
    with open(os.path.join(folder, name + ".scene"), "w") as f:
        f.write("\n".join(lines) + "\n")


def main():
    for key, g in GAMES.items():
        folder = os.path.join(HERE, "groundtruth_scene", f"{key}_pictures_c64")
        os.makedirs(folder, exist_ok=True)
        for old in os.listdir(folder):
            if old.endswith(".scene"):
                os.remove(os.path.join(folder, old))
        for room in range(1, g["rooms"] + 1):
            scene(folder, g, f"room{room:02d}", f"room {room}", room)
        for item, room in g["items"].items():
            scene(folder, g, f"item{item:02d}", f"item {item} in room {room}", room, [(item, room)], item)
        print(f"{folder}: {g['rooms']} rooms, {len(g['items'])} items")


main()
