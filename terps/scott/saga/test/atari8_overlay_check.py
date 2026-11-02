#!/usr/bin/env python3
"""Find Atari 8-bit overlays whose colours differ from the room under them.

On the original machine an overlay's colours go into the same colour registers
as the room's, so the whole picture changes and no edge shows. Spatterlight
only paints the overlay's own pixels with them, so an orange that differs a
little from the room's is visible. atari8overrides[] in
common_sagadraw/c64a8draw.c makes such values equal; this finds the rest.

    atari8_overlay_check.py [--all] [--near N] [--draw c64a8draw.c]
                            [--scott GAME.atr]... [--plus GAME.atr]...

Needs build/extract_images, build/c64a8test and ../../../plus/plus_image_probe.
Only a pair where both pictures have pixels of the colour slot is reported.
--draw takes the overrides from another version of c64a8draw.c.

How sure a pair is, best first:

  table     Plus: the game's own table says this object picture goes in this room
  start     Scott: the item starts in the room
  moved     Scott: an action can put the item in the room
  special   Scott: an overlay from DrawGameSpecificOverlays() in saga.c
  portable  Scott: an item the player can carry, if it is dropped in the room
  inventory Scott: drawn on the inventory screen, room picture 98

"edge" counts overlay pixels touching room pixels of the same slot: those show
a visible seam. A mismatch with no edge still shows as two shades in one scene.
"""
import argparse
import math
import os
import re
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from c64_decode_png import render_grid, UNSET

TERPS = os.path.normpath(os.path.join(HERE, '..', '..', '..'))
DRAW = os.path.join(TERPS, 'common_sagadraw', 'c64a8draw.c')
PALETTE = os.path.join(TERPS, 'common_sagadraw', 'atari8_palette.h')
EXTRACT = os.path.join(HERE, 'build', 'extract_images')
RENDER = os.path.join(HERE, 'build', 'c64a8test')
PROBE = os.path.join(TERPS, 'plus', 'plus_image_probe')

# The rooms and items of DrawGameSpecificOverlays() in saga.c:
# (overlay, item, room or None for wherever the item can be, or 'initial')
SPECIAL = {
    'hulk': [(70, 18, 'initial'), (72, 21, 'initial'), (13, 14, None), (13, 15, None)],
    'count': [(80, 17, 8), (81, 35, 18), (82, 5, 9)],
    'voodoo': [(80, 45, 14)],
}
# USRoomImage() in saga.c: room -> picture
HULK_ALIASES = {5: 3, 6: 3, 7: 4, 8: 4, 10: 9, 11: 9, 13: 2, 14: 2, 17: 16, 18: 16}

# Values for finding which slot each pixel uses
PROBE_VALUES = (0x42, 0x82, 0xc2)

# How many parameters each opcode takes off the action's list
ARITY = {52: 1, 53: 1, 54: 1, 55: 1, 58: 1, 59: 1, 60: 1, 62: 2, 72: 2, 74: 1, 75: 2,
         79: 1, 81: 1, 82: 1, 83: 1, 87: 1, 90: 1}
ANYWHERE = 'anywhere'


def load_colours(draw_path):
    with open(PALETTE) as f:
        text = re.sub(r'/\*.*?\*/|//[^\n]*', '', f.read(), flags=re.S)
    table = [int(x, 16) for x in re.findall(r'0x[0-9a-fA-F]{6}\b', text)]
    if len(table) != 256:
        sys.exit('%s: expected 256 colours, found %d' % (PALETTE, len(table)))
    with open(draw_path) as f:
        block = re.search(r'atari8overrides\[\] = \{(.*?)\};', f.read(), re.S).group(1)
    overrides = {int(v, 16): int(c, 16)
                 for v, c in re.findall(r'\{\s*(0x[0-9a-f]+),\s*(0x[0-9a-f]+)\s*\}', block)}
    return table, overrides


def translate(value, table, overrides):
    return overrides.get(value, table[value & 0xfe])


def distance(a, b):
    return math.sqrt(sum(((a >> s & 255) - (b >> s & 255)) ** 2 for s in (16, 8, 0)))


class Picture:
    """A picture's colour values, and which slot each of its pixels uses."""

    def __init__(self, path, voodoo, built, tmp):
        with open(path, 'rb') as f:
            self.data = bytearray(f.read())
        self.values = tuple(self.data[8:11])
        probe = bytearray(self.data)
        probe[8:11] = bytes(PROBE_VALUES)
        blob = os.path.join(tmp, 'p.dat')
        spec = os.path.join(tmp, 'p.spec')
        with open(blob, 'wb') as f:
            f.write(probe)
        with open(spec, 'w') as f:
            f.write('!atari8\n%sp.dat\n' % ('!voodoo\n' if voodoo else ''))
        grid, w, h = render_grid(RENDER, spec)
        lookup = {translate(v, *built): n + 1 for n, v in enumerate(PROBE_VALUES)}
        lookup[0] = 0
        self.pixels = {}
        self.counts = [0, 0, 0, 0]
        for y in range(h):
            for x in range(w):
                c = grid[y][x]
                if c != UNSET:
                    slot = lookup[c]
                    self.pixels[(x, y)] = slot
                    self.counts[slot] += 1


def compare(room, overlay):
    """(slot, room pixels left, overlay pixels, edge) for each slot both show"""
    left = list(room.counts)
    for pos in overlay.pixels:
        slot = room.pixels.get(pos)
        if slot is not None:
            left[slot] -= 1
    edges = [0, 0, 0, 0]
    for (x, y), slot in overlay.pixels.items():
        for pos in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
            if pos not in overlay.pixels and room.pixels.get(pos) == slot:
                edges[slot] += 1
    return [(s, left[s], overlay.counts[s], edges[s]) for s in (1, 2, 3)
            if left[s] > 0 and overlay.counts[s] > 0]


def scott_pairs(info, rooms, overlays):
    """{(room, overlay): class} from the items and actions of a Scott game"""
    where = {}
    portable = set()
    for item, initial, autoget in info['items']:
        where[item] = {initial}
        if autoget:
            portable.add(item)
    moves = []
    for conds, op0, op1 in info['actions']:
        params = [c // 20 for c in conds if c % 20 == 0]
        at = [c // 20 for c in conds if c % 20 == 4]
        ops = [op0 // 150, op0 % 150, op1 // 150, op1 % 150]
        for op in ops:
            take = ARITY.get(op, 0)
            args, params = params[:take], params[take:]
            if len(args) == take and op in (53, 62, 72, 75):
                moves.append((op, args, at))
    changed = True
    while changed:
        changed = False
        for op, args, at in moves:
            if op == 62:
                add = {args[0]: {args[1]}}
            elif op == 53:
                add = {args[0]: set(at) if at else {ANYWHERE}}
            elif op == 72:
                add = {args[0]: set(where.get(args[1], ())), args[1]: set(where.get(args[0], ()))}
            else:
                add = {args[0]: set(where.get(args[1], ()))}
            for item, locs in add.items():
                if item in where and not locs <= where[item]:
                    where[item] |= locs
                    changed = True
    pairs = {}

    def note(room, overlay, cls):
        if overlay in overlays:
            pairs.setdefault((room, overlay), cls)

    initial = {item: loc for item, loc, _ in info['items']}
    for item, locs in where.items():
        for room in rooms:
            if room == initial[item]:
                note(room, item, 'start')
            elif room in locs:
                note(room, item, 'moved')
    for overlay, item, room in SPECIAL.get(info['game'], ()):
        if room == 'initial':
            note(initial[item], overlay, 'special')
        elif room is not None:
            note(room, overlay, 'special')
        else:
            for r in where.get(item, ()):
                if r in rooms:
                    note(r, overlay, 'special')
    for item, locs in where.items():
        if item in portable or ANYWHERE in locs:
            for room in rooms:
                note(room, item, 'portable')
    return pairs


def load_scott(path, tmp, built):
    out = os.path.join(tmp, 'x')
    os.makedirs(out)
    subprocess.run([EXTRACT, path, out], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    info = {'items': [], 'actions': []}
    with open(os.path.join(out, 'game.txt')) as f:
        for line in f:
            w = line.split()
            if w[0] in ('game', 'variant'):
                info[w[0]] = w[1]
            elif w[0] == 'rooms':
                info['rooms'] = int(w[1])
            elif w[0] == 'item':
                info['items'].append((int(w[1]), int(w[2]), int(w[3])))
            elif w[0] == 'action':
                n = [int(x) for x in w[1:]]
                info['actions'].append((n[:-2], n[-2], n[-1]))
    pictures = {}

    def picture(usage, index):
        key = (usage, index)
        if key not in pictures:
            pictures[key] = Picture(os.path.join(out, 'atari8_u%d_i%03d.dat' % key),
                                    info['variant'] == 'voodoo', built, tmp)
        return pictures[key]

    names = [f for f in os.listdir(out) if f.startswith('atari8_u')]
    index = {}
    for f in names:
        m = re.match(r'atari8_u(\d)_i(\d+)\.dat', f)
        index.setdefault(int(m.group(1)), set()).add(int(m.group(2)))
    room_pictures = {r: HULK_ALIASES.get(r, r) if info['game'] == 'hulk' else r
                     for r in range(1, info['rooms'] + 1)}
    room_pictures = {r: p for r, p in room_pictures.items() if p in index.get(0, ())}
    room_objects = {i: (u, i) for u in (3, 1) for i in index.get(u, ())}
    result = []
    pairs = scott_pairs(info, set(room_pictures), set(room_objects))
    seen = set()
    for (room, overlay), cls in pairs.items():
        key = (room_pictures[room], overlay)
        if key not in seen:
            seen.add(key)
            result.append((cls, 'u0_i%03d' % key[0], picture(0, key[0]),
                           'u%d_i%03d' % room_objects[overlay], picture(*room_objects[overlay])))
    if 98 in index.get(0, ()):
        for u in (2, 3):
            for i in sorted(index.get(u, ())):
                result.append(('inventory', 'u0_i098', picture(0, 98), 'u%d_i%03d' % (u, i), picture(u, i)))
    return result


def load_plus(path, tmp, built):
    out = os.path.join(tmp, 'x')
    os.makedirs(out)
    subprocess.run([PROBE, path], check=True, cwd=os.path.dirname(PROBE), stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL, env=dict(os.environ, PLUS_PROBE_BLOBS=out))
    rooms = {}
    pairs = []
    with open(os.path.join(out, 'game.txt')) as f:
        for line in f:
            w = line.split()
            if w[0] == 'room':
                rooms[int(w[1])] = int(w[2])
            elif w[0] == 'objimg':
                pairs.append((int(w[1]), int(w[3])))
    pictures = {}

    def picture(name):
        file = os.path.join(out, name + '.dat')
        if not os.path.exists(file):
            return None
        if name not in pictures:
            pictures[name] = Picture(file, False, built, tmp)
        return pictures[name]

    result = []
    for name, overlay in sorted({('R%03d' % rooms[r], 'B%03d' % i) for r, i in pairs if r in rooms}):
        a, b = picture(name), picture(overlay)
        if a and b:
            result.append(('table', name, a, overlay, b))
    return result


ORDER = ['table', 'start', 'moved', 'special', 'portable', 'inventory']


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--scott', action='append', default=[])
    ap.add_argument('--plus', action='append', default=[])
    ap.add_argument('--draw', default=DRAW)
    ap.add_argument('--near', type=float, default=130, help='largest RGB distance reported (default 130)')
    ap.add_argument('--all', action='store_true', help='report every distance')
    args = ap.parse_args()

    built = load_colours(DRAW)
    table, overrides = load_colours(args.draw)
    if set(PROBE_VALUES) & set(built[1]):
        sys.exit('PROBE_VALUES collide with an override')

    total = 0
    for kind, paths, loader in (('Scott', args.scott, load_scott), ('Plus', args.plus, load_plus)):
        for path in paths:
            with tempfile.TemporaryDirectory() as tmp:
                pairs = loader(path, tmp, built)
                groups = {}
                for cls, rname, room, oname, overlay in pairs:
                    for slot, left, count, edge in compare(room, overlay):
                        rv, ov = room.values[slot - 1], overlay.values[slot - 1]
                        rc, oc = translate(rv, table, overrides), translate(ov, table, overrides)
                        d = distance(rc, oc)
                        if rc != oc and (args.all or d <= args.near):
                            g = groups.setdefault((rv, ov, rc, oc, d), [])
                            g.append((ORDER.index(cls), -edge, cls, rname, oname, slot, left, count, edge))
            print('== %s: %s (%d pairs, %d colour pairs flagged)' % (kind, os.path.basename(path), len(pairs), len(groups)))
            for (rv, ov, rc, oc, d), rows in sorted(groups.items(), key=lambda kv: (min(r[0] for r in kv[1]), kv[0][4])):
                total += 1
                rows.sort()
                print('  room $%02x %06x  overlay $%02x %06x  distance %.0f  (%d pairs)' % (rv, rc, ov, oc, d, len(rows)))
                for _, _, cls, rname, oname, slot, left, count, edge in rows[:8]:
                    print('    %-9s %s + %s  slot %d: room %d px, overlay %d px, edge %d' % (cls, rname, oname, slot, left, count, edge))
                if len(rows) > 8:
                    print('    ... and %d more' % (len(rows) - 8))
    print('%d colour pairs flagged' % total)


if __name__ == '__main__':
    main()
