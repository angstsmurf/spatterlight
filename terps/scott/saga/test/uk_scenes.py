#!/usr/bin/env python3
# Writes the scene tests (see scenetest.c) that show every picture of the older
# tile-picture Spectrum games: Gremlins (English, German, Spanish), Super Gran
# and both parts of Savage Island. One scene per picture, in
# groundtruth_scene/<game>_pictures_zx/, using the game file of <game>_zx/.
#
#   uk_scenes.py                write the scenes (needs build/scenetest)
#
# and then, for the goldens (see groundtruth_scene/README.md), for each scene:
#
#   cd ~/mame; SCENE=<scene> ./mame spectrum -snapshot <game> \
#       -autoboot_script .../zx_capture.lua -nothrottle -video none -sound none \
#       -seconds_to_run 3000
#
# with -cass1 in place of -snapshot for the two tape games.
#
# As in seas_scenes.py, each scene writes the room and what has to be in it
# straight into the game state. These games only redraw on a move, though, not
# on a LOOK. So the original is made to walk there, along an exit written into
# the table for the purpose, while the interpreter is told the room.
#
#   roomNN        every room, with what is in it as the game starts
#   roomNN_bare   the same without the things there that have a picture
#   itemNNN       a thing alone in the room its picture is drawn in
#
# Gremlins' animated rooms get two checks, one for each frame (see
# GREMLINS_ANIMATIONS).

import os
import re
import subprocess

HERE = os.path.dirname(os.path.abspath(__file__))
SCENES = os.path.join(HERE, "groundtruth_scene")

# The animated rooms of Gremlins, each with two checks, one for each frame.
# None of the dangers in them is a matter of chance. The gang counts down
# counter 5 every turn it is in the room and kills at 0; it sets that to 4 when
# it turns up by itself, so a gang written into a room needs it set too. The dart
# gremlin takes two turns over a dart, on flags 19 and 20, which the first turn
# of the game has already begun: taken away for a turn, it starts over.
GANG_COUNTER = [(5, 4)]
GREMLINS_ANIMATIONS = {
    # room: (item that has to be there, or None, counters, name)
    1: (50, [], "darts"),
    3: (None, [(2, 2)], "blender"),
    6: (82, GANG_COUNTER, "gang"),
    16: (82, GANG_COUNTER, "gang"),
    17: (82, GANG_COUNTER, "gang"),
    19: (82, GANG_COUNTER, "gang"),
}
DART_GREMLIN = 50

# Where the original keeps its state: room, item table (a byte each), exit
# table (six bytes a room, from room 0). Counters follow the room at +4, two
# bytes each, as in Seas of Blood. `go` walks north.
GAMES = {
    "gremlins": dict(folder="gremlins_zx", file="gremlins.z80", room=0x6369, items=0x9b62,
                     exits=0x8ca6, go="n", start=["!hw n", "!hwnext"], src=1,
                     animations=GREMLINS_ANIMATIONS),
    "gremlins_german": dict(folder="gremlins_german_zx", file="gremlins_de.tzx", room=0x6376,
                            items=0xa3bf, exits=0x8dd8, go="GEHEN NORDEN",
                            start=["!hwwait 18000", "!hw n", "!hwnext"], src=1,
                            animations=GREMLINS_ANIMATIONS),
    "gremlins_spanish": dict(folder="gremlins_spanish_zx", file="gremlins_es.tap", room=0x63be,
                             items=0xa0f2, exits=0x8c2c, go="ANDA NORTE",
                             start=["!hwwait 18000", "!hw n", "!hwnext"], src=1,
                             animations=GREMLINS_ANIMATIONS),
    "supergran": dict(folder="supergran_zx", file="supergran.sna", room=0x6366, items=0x99d9,
                      exits=0x899e, go="n", start=["!hw n"], src=1),
    "savage": dict(folder="savage_zx", file="savage1.sna", room=0x6366, items=0x96b8,
                   exits=0x8aa3, go="n", start=["!hw n"], src=1,
                   extra_items={20: 8}),   # the bear on the beach (scott_display.c)
    # Part II ends at once unless given a word from the end of part I.
    "savage2": dict(folder="savage2_zx", file="savage2.sna", room=0x6366, items=0x9784,
                    exits=0x88e6, go="n", start=["!hw n", "say 474"], src=1),
}

STORE = 0   # the room for what is not in play

# Things out of the way of a scene: (game, scene) -> items to take away too.
# The torch and the gas cylinder fetch each other (actions 13 and 41).
EXTRA_MOVES = {("gremlins_german", "room15_bare"): [22], ("gremlins_spanish", "room15_bare"): [22]}
# (game, scene) -> pokes to the original only: the action that ends the game on
# entering the room, before the original has drawn it, made to test another room.
HW_ONLY = {("gremlins", "room42"): ["7b21=0400"], ("gremlins_german", "room42"): ["7b1f=0400"],
           ("gremlins_spanish", "room42"): ["7aab=0400"], ("supergran", "room39"): ["7c4d=0400"], ("savage2", "room30"): ["7bc8=0400"]}

# Pictures that are known to differ: (game, scene) -> (pixels that match, why).
INEXACT = {
    ("savage", "room11"): (17229, "a bug of the original: it draws the beach bear, picture 9, here, whose background belongs to the beach; the interpreter draws picture 13, made for this room"),
    ("savage", "item020_room08"): (21290, "a bug of the original: it draws no bear on the beach; the interpreter draws the beach bear, picture 9, whose background matches (scott_display.c)"),
    ("savage", "room24"): (15561, "a bug of the original: the action table makes this room dark, as it is in other versions and in the interpreter, but it stays lit"),
}

# Gremlins frames: which of the two the original shows first depends on what
# was typed; these scenes have the second frame first.
SWAPPED = {("gremlins", "room06"), ("gremlins", "room17"), ("gremlins", "room19"),
           ("gremlins_german", "room03"), ("gremlins_german", "room06"), ("gremlins_german", "room19"),
           ("gremlins_spanish", "room01"), ("gremlins_spanish", "room06"),
           ("gremlins_spanish", "room17"), ("gremlins_spanish", "room19")}


def tables(g):
    """Rooms and items of a game, from the interpreter itself."""
    seed = os.path.join(SCENES, g["folder"], "_tables.scene")
    with open(seed, "w") as f:
        f.write(f"!game {g['file']}\n!check x.scr\n")
    try:
        out = subprocess.run([os.path.join(HERE, "build", "scenetest"), "-t", seed],
                             stdout=subprocess.PIPE, stderr=subprocess.DEVNULL).stdout
    finally:
        os.remove(seed)
    text = out.decode("latin-1")
    rooms = {int(m[0]): int(m[1]) for m in re.findall(r"^room (\d+) image (\d+)", text, re.M)}
    items = {int(m[0]): (int(m[1]), int(m[2]), int(m[3]), m[4]) for m in
             re.findall(r'^item (\d+) image (\d+) flag (\d+) loc (\d+) "(.*)"', text, re.M)}
    return rooms, items


def scene(gamename, g, name, title, room, moves=(), counters=(), animated=False, before=()):
    folder = os.path.join(SCENES, gamename + "_pictures_zx")
    lines = [f"# {gamename}: {title}", "# (written by uk_scenes.py)"]
    matching, why = INEXACT.get((gamename, name), (None, None))
    if matching:
        lines.append(f"# Not exact: {why}.")
    lines += [f"!game ../{g['folder']}/{g['file']}"] + g["start"] + ["!terp LOOK"]
    for item, to in before:
        lines += [f"!hwpoke {g['items'] + item:04x}={to:02x}", f"!item {item} {to}"]
    moves = list(moves) + [(i, STORE) for i in EXTRA_MOVES.get((gamename, name), [])]
    lines += [f"!hwpoke {poke}" for poke in HW_ONLY.get((gamename, name), [])]
    if before:
        lines += [f"!hwpoke {g['exits'] + 6 * g['src']:04x}={g['src']:02x}", f"!hw {g['go']}", "!terp LOOK"]
    for item, to in moves:
        lines += [f"!hwpoke {g['items'] + item:04x}={to:02x}", f"!item {item} {to}"]
    for counter, value in counters:
        lines += [f"!hwpoke {g['room'] + 4 + 2 * counter:04x}={value:02x}",
                  f"!counter {counter} {value}"]
    exit_north = g["exits"] + 6 * g["src"]
    lines += [f"!hwpoke {g['room']:04x}={g['src']:02x}",
              f"!hwpoke {exit_north:04x}={room:02x}",
              f"!hw {g['go']}", f"!room {room}", "!terp LOOK"]
    check = " min=%d" % matching if matching else ""
    if animated:
        first, second = name + "_1.scr", name + "_2.scr"
        if (gamename, name) in SWAPPED:
            first, second = second, first
        lines += ["!hwnext", f"!dump {first}", "!hwnext", f"!dump {second}",
                  f"!check {name}_1.scr{check}", "!tick 1", f"!check {name}_2.scr{check}"]
    else:
        lines.append(f"!check {name}.scr{check}")
    lines.append("")
    with open(os.path.join(folder, name + ".scene"), "w") as f:
        f.write("\n".join(lines) + "\n")


def main():
    for gamename, g in GAMES.items():
        folder = os.path.join(SCENES, gamename + "_pictures_zx")
        os.makedirs(folder, exist_ok=True)
        for name in os.listdir(folder):
            if name.endswith(".scene"):
                os.remove(os.path.join(folder, name))
        rooms, items = tables(g)
        animations = g.get("animations", {})
        # (image, flag, loc, text): drawn in room flag & 0x7f
        pictured = {i: (it[1] & 0x7f, it[2], it[3]) for i, it in items.items()
                    if it[0] and it[1] & 0x7f}

        for room, image in rooms.items():
            if room == 0 or image == 255:
                continue
            anim = animations.get(room)
            moves = []
            counters = []
            if anim:
                item, counters, _ = anim
                if item is not None and items[item][2] != room:
                    moves = [(item, room)]
            if room == 3 and anim:
                # What the running blender does to the gremlin on it, done.
                moves = [(81, STORE), (59, 3)]
            before = []
            if anim and anim[0] == DART_GREMLIN:
                before, moves = [(DART_GREMLIN, STORE)], [(DART_GREMLIN, room)]
            scene(gamename, g, f"room{room:02}", f"room {room} as the game starts", room,
                  moves=moves, counters=counters, animated=bool(anim), before=before)
            here = [i for i, (drawn, at, _) in pictured.items() if at == room and drawn == room]
            if here:
                scene(gamename, g, f"room{room:02}_bare",
                      f"room {room} without the things in it with a picture", room,
                      moves=[(i, STORE) for i in here])

        for item, (drawn, at, text) in pictured.items():
            if rooms.get(drawn, 255) == 255:
                continue
            others = [i for i, (d, a, _) in pictured.items() if a == d == drawn and i != item]
            if at == drawn and not others:
                continue    # roomNN shows it
            if animations.get(drawn, (None,))[0] == item:
                continue    # so does its animation
            scene(gamename, g, f"item{item:03}", f"{text} (item {item}) alone in room {drawn}",
                  drawn, moves=[(i, STORE) for i in others] + ([(item, drawn)] if at != drawn else []))

        for item, room in g.get("extra_items", {}).items():
            scene(gamename, g, f"item{item:03}_room{room:02}",
                  f"{items[item][3]} (item {item}) in room {room}", room, moves=[(item, room)])


if __name__ == "__main__":
    main()
