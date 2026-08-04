#!/usr/bin/env python3
"""Convert a SCARE-written ADRIFT 4.0 .tas save into ADRIFT 3.9 layout.

SCARE now writes and reads this layout natively -- saving a 3.8/3.9 game
produces a file run390.exe accepts, and restoring auto-detects either format
(scserial.cpp, ser_save_game()).  So this script is no longer part of any
workflow.  It is kept as the independent oracle that proves the native writer:
diff a native save against this script's conversion of the same state and they
must be byte-identical (they are, on The Hangover, Melbourne Beach, thetest
and circus).  It also still converts a 4.0-format save SCARE wrote earlier.

Format notes (reverse-engineered 2026-08-03 by diffing run390.exe's own saves of
The Hangover and Melbourne Beach against SCARE's).  Both converted saves restore
cleanly in run390.exe -- "Loading game... done.", right room, right score, right
inventory -- and a converted turn-0 save matches the Runner's own byte for byte
apart from the fields that genuinely differ (turn counter, elapsed seconds, a
ticking event timer):

  container   3.9 = the plain text XORed with the VB6 PRNG keystream starting
              at index 0 (unlike a .taf, whose keystream is advanced past the
              14-byte version header).  4.0 = a bare zlib stream.
  header      3.9 has no "\\xAC400052" version line and no PlayerName line.
              Everything from the room count through the room-seen flags is
              identical.
  objects     4.0 record: location (a space-padded room list for a static, a
              bare position for a dynamic), seen, parent, [openness], [state],
              unmoved.
              3.9 record: position (always bare -- an unmoved static is -1),
              seen, parent, [openness].  No unmoved flag, and openness is
              written space-padded (VB "Print #") with 5 and 6 exchanged, the
              same open/closed swap sctafpar.cpp applies when reading a 3.9
              .taf (parse_fixup_v390_object).
  battle      a Battle System game interleaves a battle block into the player
              record (between the encumbrance fields and the room-seen flags)
              and into every NPC record (after "seen", before the walk steps).
              4.0 writes each of the four ranged attributes as max/hi/lo; 3.9
              knows only the strength-vs-defence model, and writes max/hi for
              Strength and Defence alone.  So the player block is 7 values
              rather than 15, and an NPC's is 9 rather than 17.  3.9 writes
              them bare, not space-padded.
  NPCs        3.9 stores only location and "seen" (plus the battle block).  It
              does NOT store the walk-step counters 4.0 writes after them, so
              a restored 3.9 game restarts every NPC walk -- verified against a
              run390 save of Melbourne Beach, whose three walking NPCs have
              counters -1/0, 0 and 100 in the 4.0 stream and nothing at all in
              the 3.9 one.
  tail        tasks, events, variables, elapsed seconds and turns are
              byte-identical.

Usage:

    printf 'wait\\nquit\\ny\\n' | SCR_SKIP_WAITKEY=1 SCR_DUMP_OBJLOC=1 \\
        scare game.taf 2>objloc.txt >/dev/null
    python3 tas40to39.py save40.tas save39.tas objloc.txt

The dump supplies the static/openable map, which a 4.0 object record cannot be
walked without (its length varies with those two properties), plus the SAVEINFO
and NPCINFO lines that give the battle flag and each NPC's walk-step count --
neither of which is recoverable from the stream itself.  It needs one turn of
play to be emitted, hence the "wait".
"""
import sys
import re
import zlib


def keystream(n):
    """The VB6 Rnd keystream ADRIFT obfuscates 3.9/3.8 files with."""
    s = 0x00a09e86
    out = bytearray()
    for _ in range(n):
        s = (s * 0x43fd43fd + 0x00c39ec3) & 0x00ffffff
        out.append((255 * s) // 0x1000000)
    return bytes(out)


def obfuscate(data):
    return bytes(a ^ b for a, b in zip(data, keystream(len(data))))


deobfuscate = obfuscate  # XOR is its own inverse


def parse_objloc(path):
    """The static/openable map, battle flag and walk-step counts from a dump."""
    info = {'statics': set(), 'openables': set(), 'battle': 0, 'walksteps': []}
    for line in open(path, errors='replace'):
        m = re.search(r'OBJLOC obj=(\d+).*? static=(\d+).*? open=(-?\d+)', line)
        if m:
            obj, static, openness = (int(g) for g in m.groups())
            if static:
                info['statics'].add(obj)
            if openness:
                info['openables'].add(obj)
            continue
        m = re.search(r'SAVEINFO battle=(\d+)', line)
        if m:
            info['battle'] = int(m.group(1))
            continue
        m = re.search(r'NPCINFO npc=(\d+) walksteps=(\d+)', line)
        if m:
            info['walksteps'].append(int(m.group(2)))
    return info


# Which of the 4.0 battle block's values survive into 3.9, by index.  4.0 order
# is [attitude (NPC only),] max stamina, live stamina, then (max, hi, lo) for
# Strength, Defence, Accuracy and Agility, then the wielded object (player) or
# speed and attack counter (NPC) -- see ser_save_battle_block in scserial.cpp.
BATTLE_KEEP_PLAYER = [0, 1, 2, 3, 5, 6, 14]
BATTLE_KEEP_NPC = [0, 1, 2, 3, 4, 6, 7, 15, 16]


def convert(text, statics, openables, battle=0, walksteps=()):
    lines = text.split('\r\n')
    if lines and lines[-1] == '':
        lines.pop()

    pos = 0

    def take():
        nonlocal pos
        value = lines[pos]
        pos += 1
        return value

    if not take().startswith('\xac'):
        raise SystemExit('not a SCARE/Runner 4.0 save (no version line)')

    def take_battle(keep):
        """Rewrite one battle block, dropping the fields 3.9 has no room for."""
        block = [take() for _ in range(17 if keep is BATTLE_KEEP_NPC else 15)]
        return [block[i].strip() for i in keep]

    out = [take()]                                  # GameName
    counts = [take() for _ in range(5)]             # rooms/objects/tasks/events/npcs
    out += counts
    nroom, nobj, ntask, nevent, nnpc = (int(c) for c in counts)
    out.append(take())                              # score
    take()                                          # PlayerName: absent in 3.9
    out += [take() for _ in range(8)]               # player + encumbrance block
    if battle:
        out += take_battle(BATTLE_KEEP_PLAYER)
    out += [take() for _ in range(nroom)]           # room-seen flags

    for obj in range(nobj):
        if obj in statics:
            count = int(take())
            for _ in range(count):
                take()
            # 3.9 stores gs_object_position directly, and for a static that is
            # -1.  A static an event has relocated does carry a real 1-based
            # room there, but SCARE writes that case as the same one-entry room
            # list an unmoved single-room static gets, so the two are not
            # distinguishable from the 4.0 stream; -1 is right for every
            # unmoved static, which is the overwhelmingly common case.
            location = '-1'
        else:
            location = take()
        seen, parent = take(), take()
        out += [location, seen, parent]
        if obj in openables:
            openness = int(take())
            if openness in (5, 6):                  # 3.9 swaps open and closed
                openness = 11 - openness
            out.append(' %d ' % openness)
        take()                                      # unmoved flag: absent in 3.9

    out += [take() for _ in range(2 * ntask)]       # done, scored
    out += [take() for _ in range(4 * nevent)]      # time, task, state-1, taskdone

    for npc in range(nnpc):
        out += [take(), take()]                     # location, seen
        if battle:
            out += take_battle(BATTLE_KEEP_NPC)
        for _ in range(walksteps[npc] if npc < len(walksteps) else 0):
            take()                                  # walk steps: absent in 3.9

    out += lines[pos:]                              # variables, elapsed, turns
    return '\r\n'.join(out) + '\r\n'


def main():
    if len(sys.argv) != 4:
        raise SystemExit(__doc__)
    src, dst, dump = sys.argv[1:]
    info = parse_objloc(dump)
    text = zlib.decompress(open(src, 'rb').read()).decode('latin-1')
    converted = convert(text, info['statics'], info['openables'],
                        info['battle'], info['walksteps'])
    open(dst, 'wb').write(obfuscate(converted.encode('latin-1')))
    print('%s -> %s (%d static, %d openable objects, battle=%d)'
          % (src, dst, len(info['statics']), len(info['openables']),
             info['battle']))


if __name__ == '__main__':
    main()
