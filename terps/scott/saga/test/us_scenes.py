#!/usr/bin/env python3
# Writes the scene tests (see scenetest.c) that show every picture of the C64
# US releases of The Hulk and The Sorcerer of Claymorgue Castle (saga/saga.c,
# the SAGA bitmap games): groundtruth_scene/hulkus_pictures_c64/ and
# claymorgueus_pictures_c64/, one scene per picture, with the US disk next to
# them (the hulk_c64/ and claymorgue_c64/ scenes play the UK tile games,
# other disks). The aio_scenes.py of these games.
#
#   us_scenes.py hulk           write the scenes of one game
#   us_scenes.py claymorgue
#
# and then, for the goldens (see groundtruth_scene/README.md):
#
#   for s in groundtruth_scene/hulkus_pictures_c64/*.scene; do
#       ./c64_capture.py $s; done
#
# The first capture boots the game, a minute and a half, and leaves a snapshot
# of the machine (`!hwsnap`); the others start from it and take seconds.
#
# Each scene starts the game and writes the room, and what has to be in it,
# straight into the game state: with `!hwpoke` in the original, and with
# `!room`, `!item` and `!flag` in the interpreter. A LOOK then draws the room
# on both.
#
#   roomNN       every room, with what is in it as the game starts (rNNNNN
#                and the bNNNNNr overlays of the objects there), but for
#                Claymorgue's rabid rats
#   dark         the dark room, rNN000
#   itemNN       an object with a room picture that is not in play as the
#                game starts, put in a room and drawn there
#   invNN        an object with an inventory picture, carried, and INVENTORY
#                (rNN098 with the bNNNNNi overlay); invnone carries nothing
#   pictureNN    the close-ups and the title, by number: the original keeps
#                a table of (room, picture) pairs at $7a72, and its first
#                entry is made to point room 1 at the picture; the
#                interpreter is told the picture number (`!picture`)
#   bitelip, mirror, nightmare, dome, dig (Hulk)
#                the close-ups the game shows for an action, shown by the
#                action: a check that the game shows them when it should,
#                where the pictureNN scenes only check the drawing
#
# Both games run the same engine: the room number is at $1c22, the item
# locations a word each from $1c3c, item 0 first (255 = carried), the bit
# flags a byte each from $1c02, and the room is redrawn after a LOOK when
# $1f20 is set (found by diffing `!hwmem` dumps). The original draws the
# picture 24 pixels further left and 3 lines lower than the interpreter,
# hence dx=24 dy=-3; the two pictures that are 287 pixels wide instead of
# 279 (Hulk's dark room and limbo) are drawn 4 pixels further right than
# that, dx=28.
#
# The original's palette is global: each picture's four colours are written
# into the whole screen and colour RAM when it is drawn, so a room with
# overlays ends up in the colours of the last overlay drawn (the code before
# the two 16-entry colour tables at $7b92 and $7ba2, which are the ones in
# c64a8draw.c). The interpreter keeps each picture's own colours; the scenes
# where that shows get a min= and a comment.

import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))

ROOM = 0x1c22
ITEMS = 0x1c3c
FLAGS = 0x1c02
REDRAW = 0x1f20
PICTURES = 0x7a72

# The "Rage" of The Hulk, item 1: carried when Banner is the Hulk.
RAGE = ("item", 1, 255)

HULK = {
    "title": "The Hulk (US)",
    "game": "hulk.d64",
    "folder": "hulkus_pictures_c64",
    "snapshot": "hulkus.vsf",
    "rooms": 20,
    # The boot: the title, three <HIT RETURN>s, n to "restore a saved
    # game?", and the first room.
    "start": ["!hwwait 45", "!hw", "!hwwait 4", "!hw", "!hwwait 4", "!hw", "!hwwait 20",
              "!hw n", "!hwwait 8", "!hw", "!hwwait 8"],
    # The dark room: a room and the bit flag that makes it dark.
    "dark": (2, 15),
    # The room the inventory scenes stand in: the game refuses INVENTORY in
    # room 1, where Banner is tied to a chair, and in the dark. (The game
    # starts with Banner in the chair; an action there is the way out, BITE
    # LIP, but a poked room is left as it is.)
    "inventory room": 3,
    # Rooms the original moves out of on the turn that draws them.
    "skipped rooms": {
        18: "the warp moves the player to a field on the same turn, and the field is drawn",
    },
    # What a room scene pokes first.
    "room steps": {},
    # Rooms where the original does not redraw after a LOOK on the turn
    # the room is poked: an empty line instead runs the turn, the game's
    # automatic actions describe the room, and the picture is drawn.
    "look by empty line": {7},
    # Scenes where that turn's text fills the five lines of the text window:
    # the original waits for a key before it scrolls and draws the picture,
    # so another empty line goes first (the large hole in the field: "Ants
    # swarm from holes").
    "scroll pause": {"item14"},
    # Pictures 287 pixels wide, drawn 4 pixels further right.
    "wide": {"dark", "room20"},
    # Objects with a room picture that are not in play as the game starts,
    # and the room each is drawn in, with what the room needs first.
    "item scenes": {
        13: (4, []),             # large hole, dug in room 4
        14: (7, []),             # large hole, dug in room 7
        15: (8, []),             # large hole, dug in room 8
        # Item 20, the ants, is not here: they swarm into room 7 on the
        # first turn there, so room07 shows them.
        26: (14, []),            # Dr Strange, who turns up in room 14
        34: (2, [("item", 24, 0)]),  # the open hole, in place of the iron ring
        39: (15, []),            # the hole in the ceiling
        40: (15, []),            # the crack, scratched in the wall
        50: (3, []),             # a gem, which has no picture: see below
    },
    # Items whose room picture file is a placeholder: B01250R is 65 bytes,
    # three solid colour bars, on every US C64 release, not a picture of the
    # gem (and there is no B01050R). Neither the original nor the interpreter
    # draws it; the scene checks that nothing is drawn.
    "placeholder": {50: "B01250R, 65 bytes of three solid colour bars"},
    # Objects with an inventory picture (b01NNNi).
    "inventory items": [1, 2, 3, 4, 12, 16, 18, 19, 21, 23, 29, 30, 31, 32, 35, 37, 42,
                        44, 46, 48, 50, 51],
    # The close-ups and the title, by number.
    "pictures by number": list(range(81, 98)) + [99],
    # The close-ups shown for an action. Each: name, what, the state first,
    # the lines typed, with the picture checked after each line that shows
    # one. The lines after BITE LIP are the <HIT RETURN>s of the close-ups.
    "action scenes": [
        ("bitelip", "BITE LIP: the transformation close-ups 84, 83, 86", [],
         [("BITE LIP", 84), ("", 83), ("", 86), ("", None)]),
        ("mirror", "LOOK MIRROR as Banner (82) and as the Hulk (81), free and gassed in room 2",
         [], [("BITE LIP", None), ("", None), ("", None), ("", None),
              ("LOOK MIRROR", 82), ("", None), RAGE, ("LOOK MIRROR", 81), ("", None)]),
        # In the chair, made the Hulk by RAGE: the close-up is up while the
        # game waits for RETURN after "very mad", and the check comes
        # before the empty line that answers it (c64_capture leaves the
        # pause alone when the next line is empty). The original's input
        # line takes 14 characters, hence the short words.
        ("nightmare", "REMEMBER NIGHTMARE as the Hulk (88)",
         [RAGE], [("REMEM NIGHT", 88), ("", None)]),
        ("dome", "LIFT DOME in room 8 as the Hulk (85)",
         [RAGE, ("room", 8)], [("LIFT DOME", 85), ("", None)]),
        ("dig", "DIG in room 4 as the Hulk (89)",
         [RAGE, ("room", 4)], [("DIG", 89), ("", None)]),
    ],
    # The scenes the global palette (see the top) makes inexact: the room
    # is in the colours of the last overlay drawn.
    "inexact": {
        "room02": (98, "the room's orange is red, the colour of the ring (B01024R), on the original"),
        "room12": (99, "the room's light blue is blue, the colour of the overlays, on the original"),
        "room13": (99, "the bees are orange, the colour of the wax (B01072R), on the original"),
        "item34": (94, "the hole's brown is red, the colour of the ring (B01024R), on the original"),
    },
    # Where the interpreter's colours differ from the original's on purpose.
    "deviations": {},
}

CLAYMORGUE = {
    "title": "The Sorcerer of Claymorgue Castle (US)",
    # QUESTPR1.D64 with the error bytes its copy protection looks for (see
    # the registry row in ai_uk/c64decrunch.c).
    "game": "clay.d64",
    "folder": "claymorgueus_pictures_c64",
    "snapshot": "clayus.vsf",
    "rooms": 32,
    # The boot: the first file on the disk is a BASIC loader that autostart
    # loads to the wrong address, so the disk is attached and LOADed by
    # hand; then the title, three <HIT RETURN>s, n to "restore a saved
    # game?", and the first room.
    "start": ["!hwattach", "!hwwait 5", '!hw load"saga",8', "!hwwait 30", "!hw run", "!hwwait 15",
              "!hw", "!hwwait 10", "!hw", "!hwwait 10", "!hw", "!hwwait 15", "!hwkey n",
              "!hwwait 10", "!hw", "!hwwait 8"],
    # The plain room (10), empty, and the bit flag that makes it dark.
    "dark": (10, 15),
    "inventory room": 1,
    "skipped rooms": {},
    # The rabid rats (item 75) start on the staircase, and their picture
    # recolours the whole of it (the original's palette is global), so the
    # staircase is shown without them; the rats have no room whose colours
    # are theirs, and are not shown at all (see groundtruth_scene/README.md).
    "room steps": {14: [("item", 75, 0)]},
    "look by empty line": set(),
    "scroll pause": set(),
    "wide": set(),
    # Each in the room it replaces something in, where that is known; the
    # three holes, whose rooms are not, in an empty room with the hole's
    # colours: the plain room (10) for two, the dragon's lair (29) for the
    # third.
    "item scenes": {
        4: (1, [("item", 3, 0)]),     # the lowered drawbridge, for the raised one
        26: (7, [("item", 25, 0)]),   # the fallen chandelier, for the hanging one
        30: (12, [("item", 29, 0)]),  # the open oak door of the dungeon cell
        32: (13, [("item", 31, 0)]),  # the open oak door of the anteroom
        39: (2, [("item", 44, 0)]),   # the bottom of the moat, for the moat
        47: (10, []),                 # a hole
        49: (10, []),                 # a hole
        54: (24, [("item", 53, 0)]),  # the open stone door of the dusty room
        60: (29, [("item", 58, 0)]),  # a hole, without the dragon, which kills
    },
    # B13250R and B13938R are not scenes: neither game draws them (fire
    # bricks, item 50, and the loft, item 38, want B13050R and B13038R).
    "placeholder": {},
    "inventory items": [0] + list(range(5, 18)) + [20, 24, 28, 33, 34, 35, 36, 40, 41, 42, 45, 48,
                                                   50, 51, 52, 57, 59, 61, 62, 63, 64, 65, 66, 68,
                                                   69, 70, 71, 73],
    # The title. Claymorgue has no close-ups.
    "pictures by number": [99],
    "action scenes": [],
    "inexact": {
        "item32": (96, "the anteroom's brown is orange, the colour of the open door (B13032R), on the original"),
    },
    "deviations": {
        "item30": (97, "the open door's yellow is white in the interpreter"),
        "room14": (96, "the staircase's white is blue in the interpreter"),
        "room18": (91, "the planks' green is yellow in the interpreter"),
        "room20": (94, "the water's white is blue in the interpreter"),
    },
}

GAMES = {"hulk": HULK, "claymorgue": CLAYMORGUE}


def head(game, what):
    # The interpreter asks for a first line that the game never reads
    # (`!terp` types nothing on the original).
    return [f"# {game['title']}: {what}", "# (written by us_scenes.py)", f"!game {game['game']}", "!terp"] \
        + game["start"] + [f"!hwsnap {game['snapshot']}"]


def state(lines, items=(), room=None, flags=()):
    """Write a state into both games."""
    for it, to in items:
        lines += [f"!hwpoke {ITEMS + 2 * it:04x}={to:02x}", f"!item {it} {to}"]
    for flag in flags:
        lines += [f"!hwpoke {FLAGS + flag:04x}=01", f"!flag {flag}"]
    if room is not None:
        lines += [f"!hwpoke {ROOM:04x}={room:02x}", f"!hwpoke {REDRAW:04x}=01", f"!room {room}"]


def look(game, room, name):
    """The turn that draws the room."""
    if room in game["look by empty line"]:
        # The waits: with an overlay to load as well, the original's disk
        # access outlasts c64_capture's settle window, and the screen is
        # saved before the picture.
        pause = ["!hw", "!hwwait 8"] if name in game["scroll pause"] else []
        return ["!hw", "!hwwait 8"] + pause + ["!terp LOOK"]
    return ["LOOK"]


def check(game, name):
    dx = 28 if name in game["wide"] else 24
    line = f"!check {name}.vic dx={dx} dy=-3"
    for table in ("inexact", "deviations"):
        if name in game[table]:
            line += f" min={game[table][name][0]}"
    return line


def comment(game, name):
    """The comment of an inexact scene."""
    if name in game["inexact"]:
        return [f"# Inexact: {game['inexact'][name][1]}.",
                "# The original's palette is global, the last picture drawn colours all."]
    if name in game["deviations"]:
        return [f"# Inexact on purpose: {game['deviations'][name][1]}",
                "# (c64overrides in common_sagadraw/c64a8draw.c)."]
    if name in game["wide"]:
        return ["# A 287-pixel picture: the original draws it 4 pixels further right",
                "# than the others, dx=28."]
    return []


def write(folder, name, lines):
    with open(os.path.join(folder, name + ".scene"), "w") as f:
        f.write("\n".join(lines + [""]) + "\n")


def setup(lines, steps):
    """The (kind, ...) steps of the item and action scenes."""
    for step in steps:
        if step[0] == "item":
            state(lines, items=[step[1:]])
        elif step[0] == "flag":
            state(lines, flags=[step[1]])
        elif step[0] == "room":
            state(lines, room=step[1])


def main(game):
    folder = os.path.join(HERE, "groundtruth_scene", game["folder"])
    os.makedirs(folder, exist_ok=True)
    for old in os.listdir(folder):
        if old.endswith(".scene"):
            os.remove(os.path.join(folder, old))
    count = 0

    for room in range(1, game["rooms"] + 1):
        name = f"room{room:02d}"
        if room in game["skipped rooms"]:
            print(f"{name} skipped: {game['skipped rooms'][room]}")
            continue
        lines = head(game, f"room {room}")
        lines[2:2] = comment(game, name)
        setup(lines, game["room steps"].get(room, []))
        state(lines, room=room)
        write(folder, name, lines + look(game, room, name) + [check(game, name)])
        count += 1

    room, flag = game["dark"]
    lines = head(game, "the dark room")
    lines[2:2] = comment(game, "dark")
    state(lines, room=room, flags=[flag])
    write(folder, "dark", lines + look(game, room, "dark") + [check(game, "dark")])
    count += 1

    for item, (room, steps) in game["item scenes"].items():
        name = f"item{item:02d}"
        lines = head(game, f"item {item} in room {room}")
        lines[2:2] = comment(game, name)
        setup(lines, steps)
        state(lines, items=[(item, room)], room=room)
        what = check(game, name)
        if item in game["placeholder"]:
            lines[2:2] = ["# Neither draws anything for this item: its picture file on the disk",
                          f"# ({game['placeholder'][item]}) is a placeholder,",
                          "# not a picture, and there is no file with the item's own number."]
        write(folder, name, lines + look(game, room, name) + [what])
        count += 1

    for item in [None] + game["inventory items"]:
        name = "invnone" if item is None else f"inv{item:02d}"
        lines = head(game, "the inventory, carrying nothing" if item is None else f"the inventory, carrying item {item}")
        lines[2:2] = comment(game, name)
        state(lines, items=[] if item is None else [(item, 255)], room=game["inventory room"])
        write(folder, name, lines + ["INVENTORY", check(game, name)])
        count += 1

    for picture in game["pictures by number"]:
        name = f"picture{picture:02d}"
        lines = head(game, f"picture {picture}, by number")
        lines[2:2] = comment(game, name)
        lines += [f"!hwpoke {PICTURES:04x}=01{picture:02x}", f"!hwpoke {REDRAW:04x}=01", "!hw look",
                  f"!picture {picture}", check(game, name)]
        write(folder, name, lines)
        count += 1

    for name, what, steps, moves in game["action scenes"]:
        lines = head(game, what)
        setup(lines, steps)
        for move in moves:
            if move[0] in ("item", "flag", "room"):
                setup(lines, [move])
                continue
            typed, picture = move
            lines.append(typed)
            if picture:
                lines.append(check(game, f"{name}{picture}"))
        write(folder, name, lines)
        count += 1

    print(f"{folder}: {count} scenes")


if len(sys.argv) != 2 or sys.argv[1] not in GAMES:
    sys.exit(f"usage: us_scenes.py {'|'.join(GAMES)}")
main(GAMES[sys.argv[1]])
