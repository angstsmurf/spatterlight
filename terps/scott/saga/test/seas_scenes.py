#!/usr/bin/env python3
# Writes the scene tests (see scenetest.c) that show every picture of Seas of
# Blood: groundtruth_scene/seas_pictures_zx/ and seas_pictures_c64/, one scene
# per picture, next to the scenes of seas_zx/ and seas_c64/, whose game files
# they use.
#
#   seas_scenes.py              write the scenes
#
# and then, for the goldens (see groundtruth_scene/README.md):
#
#   for s in groundtruth_scene/seas_pictures_zx/*.scene; do   (in ~/mame)
#       SCENE=$s ./mame spectrum -snapshot .../seas_zx/seas.z80 \
#           -autoboot_script .../zx_capture.lua -nothrottle -video none \
#           -sound none -seconds_to_run 600; done
#   for s in groundtruth_scene/seas_pictures_c64/*.scene; do
#       ./c64_capture.py $s; done
#
# The first C64 capture loads the game and leaves a snapshot of the machine,
# seas.vsf (`!hwsnap`); the others start from it and take seconds.
#
# No script could walk to all the pictures: most rooms lie behind a dice
# battle, and what is seen from the ship depends on chance. So each scene
# starts the game and writes the room, and what has to be in it, straight into
# the game state: with `!hwpoke` in the original, and with `!room`, `!item`,
# `!counter` and `!flag` in the interpreter. A LOOK then draws the room on
# both. The LOOK is a turn like any other, so a room with an enemy ends at
# "You are attacked <HIT ENTER>", which is where the scene stops.
#
#   roomNN        every room, with what is in it as the game starts
#   roomNN_bare   the same without the things there that have a picture
#   itemNNN       a thing that is elsewhere as the game starts, or shares its
#                 room with another: alone in the room it is drawn in

import os

HERE = os.path.dirname(os.path.abspath(__file__))

# Where the original keeps its state: the room, the item table (a byte each),
# the counters (two bytes each) and the bit flags (a byte each).
PLATFORMS = {
    "zx": dict(room=0x6409, items=0x644c, counters=0x640d, flags=0x642b,
               rooms=83, golden="scr", options="",
               start=["!game ../seas_zx/seas.z80", "!hw n"]),
    "c64": dict(room=0x12c6, items=0x1309, counters=0x12ca, flags=0x12e8,
                rooms=82, golden="c64", options=" dx=32",
                start=["!game ../seas_c64/seas.d64", "!hwwait 40", "!hw", "!hwwait 40",
                       "!hwsnap seas.vsf"]),
}

SHIP = 1     # the deck of the Banshee
STORE = 10   # the room for what is not in play

# The items with a picture: where each is as the game starts, the one room
# its picture is drawn in, and its name.
ITEMS = {
    17: (1, 1, "Inhospitable Coastline"),
    19: (0, 1, "Stately Barge"),
    20: (10, 1, "Burning Barge"),
    22: (61, 61, "Salamander"),
    25: (62, 62, "A witch and cat"),
    31: (10, 63, "Basilisk with its back to you"),
    32: (59, 59, "Food laden table"),
    36: (59, 59, "Beautiful woman"),
    42: (81, 81, "Kishian Soldiers"),
    44: (0, 9, "Open stone door"),
    51: (0, 13, "Staircase"),
    54: (16, 16, "Goblins"),
    55: (17, 1, "A Wreck with a hole in the bow and one in the stern"),
    56: (66, 66, "Bark Biter"),
    59: (69, 69, "Sith Orb"),
    61: (23, 23, "Krell"),
    65: (0, 1, "Floating ice mountain"),
    67: (73, 73, "Ice Beast"),
    69: (76, 76, "Cyclops"),
    71: (32, 32, "Zombie"),
    72: (33, 33, "Ghost"),
    73: (28, 28, "Acolytes"),
    74: (28, 28, "Statue"),
    75: (26, 26, "Awkmute"),
    79: (10, 1, "Marad Galley"),
    83: (0, 33, "Ghost"),
    85: (10, 1, "Shurrapak Galley"),
    89: (0, 1, "Kishian Warship"),
    91: (75, 75, "Lizard Man"),
    92: (0, 1, "Merchant Ship"),
    93: (0, 1, "Merchant Ship"),
    94: (0, 1, "Merchant Ship"),
    95: (44, 44, "Troglodytes carrying a chest"),
    97: (0, 39, "Roc"),
    98: (0, 44, "Chest"),
    100: (0, 47, "Giant crayfish"),
    103: (49, 49, "Pirates"),
    104: (0, 49, "Treasure chest"),
    107: (0, 53, "Huge slimy creatures"),
    108: (10, 55, "The Horror"),
    124: (0, 57, "A hole"),
}

# What is seen from the ship is put there every turn by where the ship is,
# which is counters 1 and 2: the wreck is at 6, 12 and the ice mountain at
# 3, 24. A barge stays only next to item 0, which is at 6, 3.
AT_SEA = {55: (6, 12), 65: (3, 24), 19: (6, 3), 20: (6, 3)}

# Pictures that are known to differ: (platform, room) -> (pixels that match,
# why).
INEXACT = {
    ("zx", 13): (24435, "the interpreter takes the BRIGHT off four cells of the crypt on\n"
                        "# purpose (PatchCryptImage)"),
    ("c64", 28): (24544, None), ("c64", 61): (24544, None), ("c64", 69): (24544, None),
}
HALL = ("the interpreter takes a stray flip bit off the cell at the foot of the\n"
        "# outer pillar on purpose (the patch table of sagadraw.c)")


def scene(folder, p, name, title, room, moves=(), counters=(), flags=()):
    lines = [f"# Seas of Blood: {title}", "# (written by seas_scenes.py)"]
    matching, why = INEXACT.get((os.path.basename(folder)[14:], room), (None, None))
    if matching:
        lines.append(f"# Not exact: {why or HALL}.")
    # The interpreter sets the game up in its first turn, which the original
    # has behind it once it has been told not to restore a saved game.
    lines += p["start"] + ["!terp LOOK"]
    moves = list(moves)
    if room == SHIP:
        # A merchant ship that is still where it began turns up two times in
        # a hundred, so they are put away.
        moves = [(m, STORE) for m in (92, 93, 94) if m not in dict(moves)] + moves
    for item, to in moves:
        lines += [f"!hwpoke {p['items'] + item:04x}={to:02x}", f"!item {item} {to}"]
    for counter, value in counters:
        lines += [f"!hwpoke {p['counters'] + 2 * counter:04x}={value:02x}",
                  f"!counter {counter} {value}"]
    for flag in flags:
        lines += [f"!hwpoke {p['flags'] + flag:04x}=01", f"!flag {flag}"]
    check = f"!check {name}.{p['golden']}{p['options']}"
    if matching:
        check += f" min={matching}"
    # The empty line keeps the capture from answering <HIT ENTER>.
    lines += [f"!hwpoke {p['room']:04x}={room:02x}", f"!room {room}", "LOOK", check, ""]
    with open(os.path.join(folder, name + ".scene"), "w") as f:
        f.write("\n".join(lines) + "\n")


def main():
    for platform, p in PLATFORMS.items():
        folder = os.path.join(HERE, "groundtruth_scene", "seas_pictures_" + platform)
        os.makedirs(folder, exist_ok=True)
        for name in os.listdir(folder):
            if name.endswith(".scene"):
                os.remove(os.path.join(folder, name))

        for room in range(1, p["rooms"] + 1):
            if room == STORE:
                continue
            # Half the time a roc carries the player off from room 45, unless
            # flag 7 is set, and a crevasse opens in room 71, unless item 66
            # is there.
            scene(folder, p, f"room{room:02}", f"room {room} as the game starts", room,
                  moves=[(66, 71)] if room == 71 else [], flags=[7] if room == 45 else [])
            here = [i for i, (at, drawn, _) in ITEMS.items() if at == room and drawn == room]
            if here and room != SHIP:
                scene(folder, p, f"room{room:02}_bare",
                      f"room {room} without " + ", ".join(ITEMS[i][2] for i in here),
                      room, moves=[(i, STORE) for i in here])

        for item, (at, drawn, name) in ITEMS.items():
            others = [i for i, (a, d, _) in ITEMS.items() if a == d == drawn and i != item]
            if at == drawn and not others:
                continue    # roomNN shows it
            if drawn == SHIP:
                position = AT_SEA.get(item)
                scene(folder, p, f"item{item:03}", f"{name} (item {item}) from the ship", SHIP,
                      moves=[] if item in (55, 65) else [(item, SHIP)],
                      counters=[(1, position[0]), (2, position[1])] if position else [])
            else:
                scene(folder, p, f"item{item:03}", f"{name} (item {item}) alone in room {drawn}",
                      drawn, moves=[(i, STORE) for i in others] + ([(item, drawn)] if at != drawn else []))


if __name__ == "__main__":
    main()
